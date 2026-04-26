#include "tcp_server.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/version.h"
#include "esphome/components/network/util.h"
#include "esphome/components/socket/socket.h"
#include "esphome/components/socket/headers.h"

using namespace esphome;

namespace esphome {
    namespace tcp_server {

static const char *const TAG = "tcp_server";
static const size_t MAX_TX_BUFFER_SIZE = 2048;

void TCPServerComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up TCP server...");

  if (!this->tcp_buf_) {
    this->tcp_buf_ = std::unique_ptr<RingBuffer>(new RingBuffer(tcp_buf_size_, tcp_terminator_));
    ESP_LOGCONFIG(TAG, "TCP buffer size %zu, terminator '%s'", tcp_buf_size_, tcp_terminator_.c_str());
  }

  struct sockaddr_storage bind_addr;
#if ESPHOME_VERSION_CODE >= VERSION_CODE(2023, 4, 0)
  socklen_t bind_addrlen = socket::set_sockaddr_any(
      reinterpret_cast<struct sockaddr *>(&bind_addr), sizeof(bind_addr), this->port_);
#else
  socklen_t bind_addrlen = socket::set_sockaddr_any(
      reinterpret_cast<struct sockaddr *>(&bind_addr), sizeof(bind_addr), htons(this->port_));
#endif

  this->socket_ = socket::socket_ip(SOCK_STREAM, PF_INET);

  if (!this->socket_) {
    ESP_LOGE(TAG, "Failed to create TCP server socket");
    this->mark_failed();
    return;
  }

  if (this->socket_->setblocking(false) != 0) {
    ESP_LOGE(TAG, "Failed to set listener socket non-blocking: errno=%d", errno);
    this->socket_->close();
    this->socket_.reset();
    this->mark_failed();
    return;
  }
  int enable = 1;
  if (this->socket_->setsockopt(IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int)) != 0) {
    ESP_LOGW(TAG, "Failed to set TCP_NODELAY on listener: errno=%d", errno);
  }

  if (this->socket_->bind(reinterpret_cast<struct sockaddr *>(&bind_addr), bind_addrlen) != 0) {
    ESP_LOGE(TAG, "Failed to bind TCP server socket on port %u: errno=%d", this->port_, errno);
    this->socket_->close();
    this->socket_.reset();
    this->mark_failed();
    return;
  }

  if (this->socket_->listen(2) != 0) {
    ESP_LOGE(TAG, "Failed to listen on TCP server socket: errno=%d", errno);
    this->socket_->close();
    this->socket_.reset();
    this->mark_failed();
    return;
  }

}

void TCPServerComponent::loop() {
  if (this->is_failed() || !this->socket_)
    return;

  this->accept();
  if (this->has_active_clients()) {
      this->read();
      this->flush_tcp_buffer();
      this->flush_pending_writes();
  }
  this->cleanup();
}

void TCPServerComponent::on_shutdown() {
  for (Client &client : this->clients_) {
    this->close_client(client);
  }
  this->clients_.clear();

  if (this->socket_) {
    this->socket_->shutdown(SHUT_RDWR);
    this->socket_->close();
    this->socket_.reset();
  }
}

void TCPServerComponent::accept() {
    if (!this->socket_)
        return;

    struct sockaddr_storage client_addr;
    socklen_t client_addrlen = sizeof(client_addr);
    std::unique_ptr<socket::Socket> client_sock =
        this->socket_->accept(reinterpret_cast<struct sockaddr *>(&client_addr), &client_addrlen);
    if (!client_sock)
        return;

    if (client_sock->setblocking(false) != 0) {
        ESP_LOGW(TAG, "Could not set accepted socket non-blocking: errno=%d", errno);
        client_sock->close();
        return;
    }

    int enable = 1;
    if (client_sock->setsockopt(IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int)) != 0) {
        ESP_LOGW(TAG, "Could not set TCP_NODELAY on accepted socket: errno=%d", errno);
    }
#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 1, 0)
    std::string identifier = std::string{esphome::socket::SOCKADDR_STR_LEN, 0};
    auto identifier_span = std::span<char, esphome::socket::SOCKADDR_STR_LEN>(identifier.data(), identifier.size());
    identifier.resize(client_sock->getpeername_to(identifier_span));
#else
    std::string identifier = socket->getpeername();
#endif
    this->clients_.emplace_back(std::move(client_sock), identifier);

    ESP_LOGD(TAG, "New client connected: %s", identifier.c_str());
}

void TCPServerComponent::cleanup() {
  auto active = [](const Client &c) { return !c.disconnected; };
  auto cutoff = std::partition(this->clients_.begin(), this->clients_.end(), active);
  if (cutoff != this->clients_.end()) {
    for (auto it = cutoff; it != this->clients_.end(); ++it) {
      this->close_client(*it);
    }
    this->clients_.erase(cutoff, this->clients_.end());
  }
}

void TCPServerComponent::read() {
    if (!this->tcp_buf_)
        return;

    constexpr size_t buf_size = 128;
    uint8_t temp[buf_size];

    for (Client &client : this->clients_) {
        if (client.disconnected)
            continue;

        while (true) {
            ssize_t len = client.socket->read(temp, buf_size);
            if (len > 0) {
                size_t written = this->tcp_buf_->write_array(temp, len);
                if (written < static_cast<size_t>(len)) {
                    ESP_LOGW(TAG, "TCP buffer overflow — dropped %zu bytes", len - written);
                }
            } else if (len == 0 || errno == ECONNRESET || errno == ENOTCONN) {
                ESP_LOGD(TAG, "Client %s disconnected during read", client.identifier.c_str());
                client.disconnected = true;
                break;
            } else if (errno == EWOULDBLOCK || errno == EAGAIN) {
                ESP_LOGV(TAG, "No more data available from this client");
                break;
            } else {
                ESP_LOGW(TAG, "Error reading from client %s: errno=%d", client.identifier.c_str(), errno);
                client.disconnected = true;
                break;
            }
        }
    }
}

void TCPServerComponent::send_response(const std::string &response) {
    if (response.empty())
        return;

    ESP_LOGD(TAG, "Queue response %s", response.c_str());
    for (Client &client : this->clients_) {
        if (client.disconnected)
            continue;

        if (client.tx_buffer.size() + response.size() > MAX_TX_BUFFER_SIZE) {
            ESP_LOGW(TAG, "TX buffer overflow for client %s, disconnecting", client.identifier.c_str());
            client.disconnected = true;
            continue;
        }
        client.tx_buffer.append(response);
    }
    this->flush_pending_writes();
}

void TCPServerComponent::flush_tcp_buffer() {
    if (!this->tcp_buf_)
        return;

    const uint32_t now = esphome::millis();
    while (true) {
        std::string command = this->tcp_buf_->read_line();
        if (command.empty())
            break;
        this->process_command(command);
    }
    if (this->tcp_flush_timeout_ms_ > 0 &&
        (now - tcp_buf_->last_write_time()) >= this->tcp_flush_timeout_ms_ &&
        tcp_buf_->available() > 0) {
        if (this->tcp_timeout_callback_) {
            std::string partial = tcp_buf_->read_partial();
            std::string processed = this->tcp_timeout_callback_(partial);
            if (!processed.empty()) {
                ESP_LOGW(TAG, "TCP [timeout flush]: \"%s\"", processed.c_str());
                this->process_command(processed);
            } else {
                ESP_LOGW(TAG, "TCP input timed out and was discarded by lambda");
            }
        } else {
            std::string partial = tcp_buf_->read_partial();
            ESP_LOGW(TAG, "TCP input timed out without terminator — discarding partial: size=%zu", partial.size());
        }
        tcp_buf_->clear();
    }
}

void TCPServerComponent::flush_pending_writes() {
    for (Client &client : this->clients_) {
        if (client.disconnected || client.tx_offset >= client.tx_buffer.size())
            continue;

        while (client.tx_offset < client.tx_buffer.size()) {
            ssize_t sent = client.socket->write(
                reinterpret_cast<const uint8_t *>(client.tx_buffer.data()) + client.tx_offset,
                client.tx_buffer.size() - client.tx_offset);

            if (sent > 0) {
                client.tx_offset += sent;
                continue;
            }

            if (sent == 0 || errno == ECONNRESET || errno == ENOTCONN) {
                ESP_LOGD(TAG, "Client %s disconnected during write", client.identifier.c_str());
                client.disconnected = true;
                break;
            }

            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                break;
            }

            ESP_LOGW(TAG, "Error writing to client %s: errno=%d", client.identifier.c_str(), errno);
            client.disconnected = true;
            break;
        }

        if (!client.disconnected && client.tx_offset >= client.tx_buffer.size()) {
            client.tx_buffer.clear();
            client.tx_offset = 0;
        }
    }
}

void TCPServerComponent::close_client(Client &client) {
    if (!client.socket)
        return;

    client.socket->shutdown(SHUT_RDWR);
    client.socket->close();
    client.socket.reset();
    client.tx_buffer.clear();
    client.tx_offset = 0;
}


bool TCPServerComponent::has_active_clients() const {
  for (const auto &client : this->clients_) {
    if (!client.disconnected)
      return true;
  }
  return false;
}
  }  // namespace tcp_server
}  // namespace esphome