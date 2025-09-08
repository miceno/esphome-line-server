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
#include "../tcp_server/ring_buffer.h"
#include "../tcp_server/tcp_server.h"

using esphome::tcp_server::RingBuffer;
using namespace esphome;

namespace esphome {
		namespace rolloffino {

class RolloffinoComponent : public tcp_server::TCPServerComponent {
public:
    void set_duty_cycle(uint16_t duty_cycle) { duty_cycle_ = duty_cycle; }
    void set_opened_binary_sensor(binary_sensor::BinarySensor *sensor) { this->opened_binary_sensor_ = sensor; }
    void set_closed_binary_sensor(binary_sensor::BinarySensor *sensor) { this->closed_binary_sensor_ = sensor; }
    void set_in1_pin(GPIOPin *pin) { this->in1_pin_ = pin; }
    void set_in2_pin(GPIOPin *pin) { this->in2_pin_ = pin; }
    void set_max_duration(uint32_t seconds) {
        max_duration_ = seconds;
    }

    void process_command(const std::string &command) override;

    void dump_config() override;

		void loop() override;

protected:
    void handle_motor_();
    void motor_open_();
    void motor_close_();
    void motor_abort_();

    binary_sensor::BinarySensor *opened_binary_sensor_ = nullptr;
    binary_sensor::BinarySensor *closed_binary_sensor_ = nullptr;
    GPIOPin *in2_pin_ = nullptr;
    GPIOPin *in1_pin_ = nullptr;
    uint16_t duty_cycle_ = 100;

    enum MotorDirection {
        MOTOR_NONE,
        MOTOR_OPEN,
        MOTOR_CLOSE
    };
    MotorDirection motor_direction_ = MOTOR_NONE;
    bool motor_active_ = false;
    // Movement timeout in seconds
    uint32_t max_duration_ = 0;
    uint32_t motor_move_start_time_ = 0;
};

		}  // namespace rolloffino
}  // namespace esphome
