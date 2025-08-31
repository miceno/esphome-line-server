#include "rolloffino.h"

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/util.h"
#include "esphome/core/version.h"
#include "esphome/components/network/util.h"
#include "esphome/components/socket/socket.h"

using esphome::rolloffino::RingBuffer;
using namespace esphome;

static const char *const TAG = "rolloffino";
static const char *const VERSION = "V1.7-esp-wifimanager-magnet-DRV8871";

void RolloffinoComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up rollofino...");

  if (!this->tcp_buf_) {
    this->tcp_buf_ = std::unique_ptr<RingBuffer>(new RingBuffer(tcp_buf_size_, tcp_terminator_));
    ESP_LOGCONFIG(TAG, "TCP buffer was not set explicitly. Using default size %zu, terminator '%s'",
             tcp_buf_size_, tcp_terminator_.c_str());
  }

  // Setup TCP socket server
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
  this->socket_->bind(reinterpret_cast<struct sockaddr *>(&bind_addr), bind_addrlen);
  this->socket_->listen(8);

  this->publish_sensor();
}

void RolloffinoComponent::loop() {
  this->accept();
  this->read();                  // TCP → buffer
  this->flush_tcp_buffer();       // TCP buffer → processing
  this->cleanup();
  this->handle_motor_();          // Unified non-blocking motor steps
}

void RolloffinoComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Listening on: %s:%u", esphome::network::get_use_address().c_str(), this->port_);
  ESP_LOGCONFIG(TAG, "TCP buffer: size=%zu, terminator=%s",
      tcp_buf_size_,
      esphome::format_hex_pretty((const uint8_t*)tcp_terminator_.data(), tcp_terminator_.size()).c_str());
  ESP_LOGCONFIG(TAG, "TCP flush timeout: %ums", tcp_flush_timeout_ms_);
  ESP_LOGCONFIG(TAG, "Opened sensor: %s", this->opened_binary_sensor_ != nullptr ? this->opened_binary_sensor_->get_object_id().c_str() : "None");
  LOG_BINARY_SENSOR("  ", "Opened sensor:", this->opened_binary_sensor_);
  ESP_LOGCONFIG(TAG, "Closed sensor: %s", this->closed_binary_sensor_ != nullptr ? this->closed_binary_sensor_->get_object_id().c_str() : "None");
  LOG_BINARY_SENSOR("  ", "Closed sensor:", this->closed_binary_sensor_);
}

void RolloffinoComponent::on_shutdown() {
  for (const Client &client : this->clients_)
    client.socket->shutdown(SHUT_RDWR);
}

void RolloffinoComponent::publish_sensor() {
}

void RolloffinoComponent::accept() {
    struct sockaddr_storage client_addr;
    socklen_t client_addrlen = sizeof(client_addr);
    std::unique_ptr<socket::Socket> client_sock =
        this->socket_->accept(reinterpret_cast<struct sockaddr *>(&client_addr), &client_addrlen);
    if (!client_sock)
        return;

    if (!this->has_active_clients()) {
        ESP_LOGW(TAG, "No active clients connected");
    }

    client_sock->setblocking(false);
    std::string identifier = client_sock->getpeername();
    this->clients_.emplace_back(std::move(client_sock), identifier);

    ESP_LOGD(TAG, "New client connected from %s", identifier.c_str());
    this->publish_sensor();
}

void RolloffinoComponent::cleanup() {
  auto active = [](const Client &c) { return !c.disconnected; };
  auto cutoff = std::partition(this->clients_.begin(), this->clients_.end(), active);
  if (cutoff != this->clients_.end()) {
    this->clients_.erase(cutoff, this->clients_.end());
    this->publish_sensor();
  }
}

/**
 * Read data from all connected clients and writes to the TCP buffer.
 * Handles disconnections and read errors gracefully.
 */
void RolloffinoComponent::read() {
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
            } else if (len == 0 || errno == ECONNRESET) {
                ESP_LOGD(TAG, "Client %s disconnected during read", client.identifier.c_str());
                client.disconnected = true;
                break;
            } else if (errno == EWOULDBLOCK || errno == EAGAIN) {
                break;  // No more data available from this client
            } else {
                ESP_LOGW(TAG, "Error reading from client %s: errno=%d", client.identifier.c_str(), errno);
                client.disconnected = true;
                break;
            }
        }
    }
}

void RolloffinoComponent::send_response(const std::string &response) {
		if (response.empty())
				return;

		ESP_LOGD(TAG, "Send message %s", response.c_str());
		// Send response to all connected clients
		// Note: In a real application, you might want to send responses only to the
		// client that sent the command or implement a more complex routing mechanism.
		// Here, we broadcast to all connected clients for simplicity.
		// Handle partial writes and disconnections

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
						} else if (sent == 0 || errno == ECONNRESET) {
								ESP_LOGD(TAG, "Client %s disconnected during write", client.identifier.c_str());
								client.disconnected = true;
								break;
						} else if (errno == EWOULDBLOCK || errno == EAGAIN) {
								// Socket not ready for writing; could implement a retry mechanism here
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

void RolloffinoComponent::process_command(const std::string &command){
	ESP_LOGD(TAG, "Command is %s", command.c_str());

	std::string response;
	// Process command here
	if( command == "(CON:0:0)" ){
		ESP_LOGD(TAG, "Connection request");
		response = "(ACK:0:0)";
	}
	else if (command == "(GET:OPENED:0)"){
		ESP_LOGD(TAG, "Opened status");
		response = "(ACK:OPENED:";
		if (this->opened_binary_sensor_ != nullptr && this->opened_binary_sensor_->state) {
			response += "ON)";
		} else {
			response += "OFF)";
		}
	}
	else if (command == "(GET:CLOSED:0)"){
		ESP_LOGD(TAG, "Closed status");
		response = "(ACK:CLOSED:";
		if (this->closed_binary_sensor_ != nullptr && this->closed_binary_sensor_->state) {
			response += "ON)";
		} else {
			response += "OFF)";
		}
	}
	else if (command == "(SET:OPEN:0)"){
		ESP_LOGD(TAG, "Open cover");
		response = "(ACK:OPEN:ON)";
		this->motor_open_();
	}
	else if (command == "(SET:CLOSE:0)"){
		ESP_LOGD(TAG, "Close cover");
		response = "(ACK:CLOSE:ON)";
		this->motor_close_();
	}
	else if (command == "(GET:LOCKED:0)"){
		ESP_LOGD(TAG, "Locked status");
		response = "(ACK:LOCKED:OFF)";
	}
	else if (command == "(GET:AUXSTATE:0)"){
		ESP_LOGD(TAG, "Aux state");
		response = "(ACK:AUXSTATE:OFF)";
	} else {
		ESP_LOGE(TAG, "Unknown command: %s", command.c_str());
		response = "(NAK:ERROR:" + command + ")";
	}

	this->send_response(response);
}

void RolloffinoComponent::flush_tcp_buffer() {
    if (!this->tcp_buf_)
        return;

    const uint32_t now = esphome::millis();

    // Step 1: send complete lines ending in terminator
    while (true) {
        std::string command = this->tcp_buf_->read_line();
        if (command.empty())
            break;

        this->process_command(command);
    }

    // Step 2: handle stale partials
    if (this->tcp_flush_timeout_ms_ > 0 &&
        (now - tcp_buf_->last_write_time()) >= this->tcp_flush_timeout_ms_ &&
        tcp_buf_->available() > 0) {

        if (this->tcp_timeout_callback_) {
            std::string partial = tcp_buf_->read_partial();  // More appropriate than read_line()
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

        tcp_buf_->clear();  // Always clear after timeout handling
    }
}


bool RolloffinoComponent::has_active_clients() const {
  for (const auto &client : this->clients_) {
    if (!client.disconnected)
      return true;
  }
  return false;
}

void RolloffinoComponent::motor_open_() {
  // Start non-blocking open sequence
  if (this->direction_pin_ != nullptr && this->step_pin_ != nullptr) {
    // Set direction to open
    this->direction_pin_->digital_write(true);
    this->motor_direction_ = MOTOR_OPEN;
    this->motor_active_ = true;
    this->motor_steps_remaining_ = 200;
    this->motor_last_step_time_ = esphome::micros();
    this->motor_move_start_time_ = esphome::micros();
  }
}

void RolloffinoComponent::motor_close_() {
  // Start non-blocking close sequence
  if (this->direction_pin_ != nullptr && this->step_pin_ != nullptr) {
    // Set direction to close
    this->direction_pin_->digital_write(false);
    this->motor_direction_ = MOTOR_CLOSE;
    this->motor_active_ = true;
    this->motor_steps_remaining_ = 200;
    this->motor_last_step_time_ = esphome::micros();
    this->motor_move_start_time_ = esphome::micros();
  }
}

void RolloffinoComponent::motor_abort_() {
  this->motor_active_ = false;
  this->motor_direction_ = MOTOR_NONE;
  this->motor_steps_remaining_ = 0;
  if (this->step_pin_ != nullptr)
    this->step_pin_->digital_write(true);
  if (this->direction_pin_ != nullptr)
    this->direction_pin_->digital_write(true);
}

void RolloffinoComponent::handle_motor_() {
  if (!this->motor_active_ || this->motor_direction_ == MOTOR_NONE || this->motor_steps_remaining_ <= 0)
    return;

  // Abort if movement exceeds timeout
  uint32_t now = esphome::micros();
  if (now - this->motor_move_start_time_ > this->move_timeout) {
    this->motor_abort_();
    ESP_LOGW(TAG, "Motor movement aborted due to timeout");
    return;
  }

  // Step the motor if enough time has passed since last step
  if (now - this->motor_last_step_time_ >= 2000) {
    this->step_pin_->digital_write(true);
    esphome::delayMicroseconds((uint32_t)1000);
    this->step_pin_->digital_write(false);
    this->motor_steps_remaining_--;
    this->motor_last_step_time_ = now;
    if (this->motor_steps_remaining_ <= 0) {
      this->motor_active_ = false;
      this->motor_direction_ = MOTOR_NONE;
      this->motor_abort_();
    }
  }
}
