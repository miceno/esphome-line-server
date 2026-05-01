#pragma once

#include "esphome/components/cover/cover.h"
#include "esphome/components/tcp_server/tcp_server.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/hal.h"


namespace esphome {
namespace rolloffino_cover {

class RolloffinoCoverComponent : public tcp_server::TCPServerComponent, public cover::Cover {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  void process_command(const std::string &command) override;
  cover::CoverTraits get_traits() override;

  // Setters expected by the Python codegen
  void set_duty_cycle(uint16_t duty_cycle) { this->duty_cycle_ = duty_cycle; }
  void set_opened_binary_sensor(binary_sensor::BinarySensor *sensor) { this->opened_binary_sensor_ = sensor; }
  void set_closed_binary_sensor(binary_sensor::BinarySensor *sensor) { this->closed_binary_sensor_ = sensor; }
  void set_in1_pin(GPIOPin *pin) { this->in1_pin_ = pin; }
  void set_in2_pin(GPIOPin *pin) { this->in2_pin_ = pin; }
  void set_max_duration(uint32_t seconds) { this->max_duration_ = seconds; }

 protected:
  void control(const cover::CoverCall &call) override;
  void sync_cover_state_();
  void publish_assumed_entities_();

  // Motor helpers implemented locally (no compile-time dependency on rolloffino)
 protected:
  enum MotorDirection { MOTOR_NONE, MOTOR_OPEN, MOTOR_CLOSE };
  void handle_motor_();
  void motor_open_();
  void motor_close_();
  void motor_abort_();
  void motor_start_(MotorDirection direction, bool in1_state, bool in2_state);
  void check_and_abort_on_limit_();
  bool is_opened_() const;
  bool is_closed_() const;

  // optional assumed-open binary sensor
#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *assumed_open_sensor_{nullptr};
#endif

  // Motor and IO state (copied from original rolloffino)
  binary_sensor::BinarySensor *opened_binary_sensor_ = nullptr;
  binary_sensor::BinarySensor *closed_binary_sensor_ = nullptr;
  GPIOPin *in2_pin_ = nullptr;
  GPIOPin *in1_pin_ = nullptr;
  uint16_t duty_cycle_ = 100;

  MotorDirection motor_direction_ = MOTOR_NONE;
  bool motor_active_ = false;
  // Movement timeout in seconds
  uint32_t max_duration_ = 0;
  uint32_t motor_move_start_time_ = 0;
};

}  // namespace rolloffino_cover
}  // namespace esphome

