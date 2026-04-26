#pragma once
#include "../tcp_server/tcp_server.h"
#include "esphome/components/servo/servo.h"
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif

namespace esphome {
namespace snapcap {

#ifdef USE_NUMBER
class SnapCapServoPositionNumber;
#endif

class SnapCapComponent : public tcp_server::TCPServerComponent {
public:
    void process_command(const std::string &command) override;
    void dump_config() override;
    void setup() override;

    // Setters for configuration
    void set_device_id(uint8_t id) { device_id_ = static_cast<DeviceType>(id); }
    void set_brightness(uint8_t brightness) { brightness_ = brightness; }
    void set_servo_position(uint16_t position) { servo_position_ = position; }
    void set_max_degrees(uint16_t max_degrees) { max_degrees_ = max_degrees; }

    void set_servo(servo::Servo *servo) { servo_ = servo; }

#ifdef USE_NUMBER
    void set_servo_position_number(number::Number *number) { servo_position_number_ = number; }
#endif

protected:
    // Device type enum for protocol
    enum DeviceType {
        FLAT_MAN_L = 10,
        FLAT_MAN_XL = 15,
        FLAT_MAN = 19,
        FLIP_DUST = 98,
        FLIP_FLAT = 99
    };
    // Cover status enum for protocol
    enum CoverStatus {
        COVER_MOVING = 0,
        COVER_OPEN = 1,
        COVER_CLOSED = 2,
        COVER_TIMED_OUT = 3,
        COVER_OPEN_CIRCUIT = 4,
        COVER_OVERCURRENT = 5,
        COVER_USER_ABORT = 6
    };
    enum ServoStatuses {
        MS_STOPPED = 0,
        MS_RUNNING = 1,
        MS_UNDEFINED
    };

    // Device state variables
    DeviceType device_id_ = FLIP_FLAT;
    uint8_t brightness_ = 128;
    bool light_on_ = false;
    uint16_t servo_position_ = 0;
    uint16_t max_degrees_ = 270;
    CoverStatus cover_status_ = COVER_CLOSED;
    ServoStatuses servo_status_ = MS_STOPPED;
    uint8_t light_status_ = 0;
    static const char *firmware_version_;

    servo::Servo *servo_ = nullptr;

#ifdef USE_NUMBER
    number::Number *servo_position_number_ = nullptr;
    bool publishing_number_state_ = false;
#endif

    void apply_servo_position_(uint16_t position, bool write_servo);
    float servo_command_from_position_(uint16_t position) const;
    uint16_t clamp_servo_position_(uint16_t position) const;
    void publish_servo_position_();

    static constexpr uint16_t POSITION_CLOSED = 0;

    static constexpr float SERVO_POSITION_OPEN = 1.0f;
    static constexpr float SERVO_POSITION_CLOSED = -1.0f;

#ifdef USE_NUMBER
    friend class SnapCapServoPositionNumber;
#endif
};

#ifdef USE_NUMBER
class SnapCapServoPositionNumber : public number::Number {
 public:
  explicit SnapCapServoPositionNumber(SnapCapComponent *parent) : parent_(parent) {}

 protected:
  void control(float value) override;
  SnapCapComponent *parent_;
};
#endif

} // namespace snapcap
} // namespace esphome
