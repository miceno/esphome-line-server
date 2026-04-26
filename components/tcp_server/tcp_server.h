#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <algorithm>
#include <utility>
#include "esphome/core/component.h"
#include "esphome/components/socket/socket.h"
#include "esphome/components/network/util.h"
#include "esphome/components/tcp_server/ring_buffer.h"

namespace esphome {
    namespace tcp_server {

#define LOG_TCP_SERVER(prefix, type, obj) \
  if ((obj) != nullptr) { \
    ESP_LOGCONFIG(TAG, "  Listening on: %s:%u", esphome::network::get_use_address(), this->port_); \
    ESP_LOGCONFIG(TAG, "  TCP buffer: size=%zu, terminator=%s", \
      tcp_buf_size_, \
      esphome::format_hex_pretty((const uint8_t*)tcp_terminator_.data(), tcp_terminator_.size()).c_str()); \
    ESP_LOGCONFIG(TAG, "  TCP flush timeout: %ums", tcp_flush_timeout_ms_); \
  }

class TCPServerComponent : public esphome::Component {
public:
    void set_port(uint16_t port) { port_ = port; }
    void set_tcp_buffer_size(size_t size) { tcp_buf_size_ = size; }
    void set_tcp_flush_timeout(uint32_t ms) { tcp_flush_timeout_ms_ = ms; }
    void set_tcp_terminator(const std::string &term) { tcp_terminator_ = term; }
    void set_tcp_timeout_callback(std::function<std::string(const std::string &)> cb) { tcp_timeout_callback_ = std::move(cb); }

    virtual void process_command(const std::string &command) {
        // Default: echo command
        send_response(command);
    }
    void send_response(const std::string &response);

    void setup() override;
    void loop() override;
    void on_shutdown() override;
    float get_setup_priority() const override { return esphome::setup_priority::AFTER_WIFI; }

protected:
    void accept();
    void cleanup();
    void read();
    void flush_tcp_buffer();

    struct Client {
        Client(std::unique_ptr<esphome::socket::Socket> socket, std::string identifier)
            : socket(std::move(socket)), identifier(std::move(identifier)) {}
        std::unique_ptr<esphome::socket::Socket> socket;
        std::string identifier;
        std::string tx_buffer;
        size_t tx_offset = 0;
        bool disconnected = false;
    };
    void flush_pending_writes();
    void close_client(Client &client);

    uint16_t port_{};
    size_t tcp_buf_size_ = 512;
    std::string tcp_terminator_ = "\r";
    uint32_t tcp_flush_timeout_ms_ = 300;
    std::function<std::string(std::string)> tcp_timeout_callback_{};
    std::unique_ptr<esphome::tcp_server::RingBuffer> tcp_buf_;
    std::unique_ptr<esphome::socket::Socket> socket_;
    std::vector<Client> clients_;
    bool has_active_clients() const;
};

  }  // namespace tcp_server
}  // namespace esphome