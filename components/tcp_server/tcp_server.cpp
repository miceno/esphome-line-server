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
  this->socket_->setblocking(false);
  int enable = 1;
  this->socket_->setsockopt(IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int));

  this->socket_->bind(reinterpret_cast<struct sockaddr *>(&bind_addr), bind_addrlen);
  this->socket_->listen(8);
}

void TCPServerComponent::loop() {
  this->accept();
  if (this->clients_.size() > 0){
      this->read();
      this->flush_tcp_buffer();
      this->cleanup();
  }
}

void TCPServerComponent::on_shutdown() {
  for (const Client &client : this->clients_)
    client.socket->shutdown(SHUT_RDWR);
}

void TCPServerComponent::accept() {
    struct sockaddr_storage client_addr;
    socklen_t client_addrlen = sizeof(client_addr);
    std::unique_ptr<socket::Socket> client_sock =
        this->socket_->accept(reinterpret_cast<struct sockaddr *>(&client_addr), &client_addrlen);
    if (!client_sock)
        return;

    client_sock->setblocking(false);
    int enable = 1;
    client_sock->setsockopt(IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int));
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

    ESP_LOGD(TAG, "Send response %s", response.c_str());
    for (Client &client : this->clients_) {
        if (client.disconnected)
            continue;

        ssize_t total_sent = 0;
        while (total_sent < static_cast<ssize_t>(response.size())) {
            ssize_t sent = client.socket->write(
                reinterpret_cast<const uint8_t *>(response.data()) + total_sent,
                response.size() - total_sent);
            if (sent > 0) {
                total_sent += sent;
            } else if (sent == 0 || errno == ECONNRESET || errno == ENOTCONN) {
                ESP_LOGD(TAG, "Client %s disconnected during write", client.identifier.c_str());
                client.disconnected = true;
                break;
            } else if (errno == EWOULDBLOCK || errno == EAGAIN) {
                ESP_LOGW(TAG, "Socket not ready for writing to client %s", client.identifier.c_str());
                break;
            } else {
                ESP_LOGW(TAG, "Error writing to client %s: errno=%d", client.identifier.c_str(), errno);
                client.disconnected = true;
                break;
            }
        }
    }
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

bool TCPServerComponent::has_active_clients() const {
  for (const auto &client : this->clients_) {
    if (!client.disconnected)
      return true;
  }
  return false;
}
  }  // namespace tcp_server
}  // namespace esphome