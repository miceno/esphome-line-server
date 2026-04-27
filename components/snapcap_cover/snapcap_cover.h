#pragma once

#include "../tcp_server/tcp_server.h"
#include "esphome/components/cover/cover.h"
#include "esphome/components/servo/servo.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

namespace esphome {
namespace snapcap_cover {

class SnapCapCoverComponent : public tcp_server::TCPServerComponent, public cover::Cover {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  void process_command(const std::string &command) override;
  cover::CoverTraits get_traits() override;

  void set_device_id(uint8_t id) { this->device_id_ = id; }
  void set_servo(servo::Servo *servo) { this->servo_ = servo; }
  void set_open_level(float level) { this->open_level_ = level; }
  void set_closed_level(float level) { this->closed_level_ = level; }
  void set_move_duration(uint32_t ms) { this->move_duration_ms_ = ms; }
  void set_initial_opened(bool opened) { this->initial_opened_ = opened; }
#ifdef USE_BINARY_SENSOR
  void set_assumed_open_sensor(binary_sensor::BinarySensor *sensor) { this->assumed_open_sensor_ = sensor; }
#endif
#ifdef USE_TEXT_SENSOR
  void set_state_text_sensor(text_sensor::TextSensor *sensor) { this->state_text_sensor_ = sensor; }
#endif

 protected:
  void control(const cover::CoverCall &call) override;

  enum CoverStatus {
    COVER_MOVING = 0,
    COVER_OPEN = 1,
    COVER_CLOSED = 2,
    COVER_TIMED_OUT = 3,
    COVER_OPEN_CIRCUIT = 4,
    COVER_OVERCURRENT = 5,
    COVER_USER_ABORT = 6,
  };

  enum ServoStatus {
    MS_STOPPED = 0,
    MS_RUNNING = 1,
  };

  void begin_motion_(bool open);
  void abort_motion_();
  void update_motion_state_();
  void publish_assumed_entities_();
  void sync_cover_state_();

  uint8_t device_id_{99};
  servo::Servo *servo_{nullptr};
  float open_level_{1.0f};
  float closed_level_{-1.0f};
  uint32_t move_duration_ms_{1200};
  bool initial_opened_{false};

  bool target_opened_{false};
  bool current_opened_{false};
  uint32_t motion_started_at_{0};
  ServoStatus servo_status_{MS_STOPPED};
  CoverStatus cover_status_{COVER_CLOSED};

#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *assumed_open_sensor_{nullptr};
#endif
#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *state_text_sensor_{nullptr};
#endif

  static const char *firmware_version_;
};

}  // namespace snapcap_cover
}  // namespace esphome

