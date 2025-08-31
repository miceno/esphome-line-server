#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/socket/socket.h"
#include "esphome/components/rolloffino/ring_buffer.h"

using esphome::rolloffino::RingBuffer;
using namespace esphome;

class RolloffinoComponent : public esphome::Component {
public:
    void set_tcp_config(size_t size, const std::string &term) {
        tcp_buf_size_ = size;
        tcp_terminator_ = term;
    }

    void set_duty_cycle(uint16_t duty_cycle) { duty_cycle = duty_cycle; }
    void set_port(uint16_t port) { port_ = port; }
    void set_tcp_buffer_size(size_t size) { tcp_buf_size_ = size; }
    void set_tcp_flush_timeout(uint32_t ms) { tcp_flush_timeout_ms_ = ms; }
    void set_tcp_terminator(const std::string &term) { tcp_terminator_ = term; }
    std::function<std::string(const std::string &)> tcp_timeout_callback_{};
    void set_tcp_timeout_callback(std::function<std::string(const std::string &)> cb) {
        this->tcp_timeout_callback_ = std::move(cb);
    }
		void set_opened_binary_sensor(binary_sensor::BinarySensor *sensor) { this->opened_binary_sensor_ = sensor; }
		void set_closed_binary_sensor(binary_sensor::BinarySensor *sensor) { this->closed_binary_sensor_ = sensor; }
		void set_in1_pin(GPIOPin *pin) { this->in1_pin_ = pin; }
		void set_in2_pin(GPIOPin *pin) { this->in2_pin_ = pin; }

    void process_command(const std::string &command);
    void send_response(const std::string &response);

    void setup() override;
    void loop() override;
    void dump_config() override;
    void on_shutdown() override;

    float get_setup_priority() const override { return esphome::setup_priority::AFTER_WIFI; }

protected:
    void publish_sensor();
    void accept();
    void cleanup();
    void read();
    void flush_tcp_buffer();

    struct Client {
        Client(std::unique_ptr<esphome::socket::Socket> socket, std::string identifier)
            : socket(std::move(socket)), identifier(std::move(identifier)) {}
        std::unique_ptr<esphome::socket::Socket> socket;
        std::string identifier;
        bool disconnected = false;
    };

    uint16_t port_{};

    size_t tcp_buf_size_ = 512;
    std::string tcp_terminator_ = "\r";
    uint32_t tcp_flush_timeout_ms_ = 300;

    std::unique_ptr<RingBuffer> tcp_buf_;

    std::unique_ptr<esphome::socket::Socket> socket_;
    std::vector<Client> clients_;

    bool has_active_clients() const;

		binary_sensor::BinarySensor *opened_binary_sensor_;
		binary_sensor::BinarySensor *closed_binary_sensor_;
		GPIOPin *in2_pin_;
		GPIOPin *in1_pin_;

		// Max PWM duty cycle (0-255)
		uint16_t duty_cycle_ = 100;

    enum MotorDirection {
        MOTOR_NONE,
        MOTOR_OPEN,
        MOTOR_CLOSE
    };

    void motor_open_();
    void motor_close_();
    void handle_motor_();
    void motor_abort_();

    MotorDirection motor_direction_ = MOTOR_NONE;
    bool motor_active_ = false;
    bool pwm_active_ = false;
    // Movement timeout in microseconds
    uint32_t move_timeout = 10000000;
    uint32_t motor_move_start_time_ = 0;
};
