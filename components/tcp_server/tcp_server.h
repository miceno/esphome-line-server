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
#include "esphome/core/version.h"
#include "esphome/components/socket/socket.h"
#include "esphome/components/tcp_server/ring_buffer.h"

namespace esphome {
    namespace tcp_server {


class TCPServerComponent : public esphome::Component {
public:
    void set_port(uint16_t port) { port_ = port; }
    void set_tcp_buffer_size(size_t size) { tcp_buf_size_ = size; }
    void set_tcp_flush_timeout(uint32_t ms) { tcp_flush_timeout_ms_ = ms; }
    void set_tcp_terminator(const std::string &term) { tcp_terminator_ = term; }
    void set_tcp_timeout_callback(std::function<std::string(const std::string &)> cb) { tcp_timeout_callback_ = std::move(cb); }
    void set_log_tag(const char *tag) { log_tag_ = tag; }

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
    struct Client;
    void accept();
    void cleanup();
    void read();
    void flush_tcp_buffer();
    void flush_pending_writes();
    void close_client(Client &client);
    void dump_tcp_server_config_(const char *tag) const;
    std::string log_tag_ = "tcp_server";

    // in public section
    struct Client {
        Client(std::unique_ptr<esphome::socket::Socket> socket, std::string identifier)
            : socket(std::move(socket)), identifier(std::move(identifier)) {}
        std::unique_ptr<esphome::socket::Socket> socket;
        std::string identifier;
        std::string tx_buffer;
        size_t tx_offset = 0;
        bool disconnected = false;
    };

    uint16_t port_{};
    size_t tcp_buf_size_ = 512;
    std::string tcp_terminator_ = "\r";
    uint32_t tcp_flush_timeout_ms_ = 300;
    std::function<std::string(std::string)> tcp_timeout_callback_{};
    std::unique_ptr<esphome::tcp_server::RingBuffer> tcp_buf_;
#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 3, 0)
    esphome::socket::ListenSocket *socket_{nullptr};
#else
    std::unique_ptr<esphome::socket::Socket> socket_;
#endif

    std::vector<Client> clients_;
    bool has_active_clients() const;
};

  }  // namespace tcp_server
}  // namespace esphome