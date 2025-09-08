#pragma once
#include "esphome/components/tcp_server/tcp_server.h"
#include "esphome/components/servo/servo.h"

namespace esphome {
namespace snapcap {

class SnapCapComponent : public tcp_server::TCPServerComponent {
public:
    void process_command(const std::string &command) override;
    void dump_config() override;
    void setup() override;

    // Setters for configuration
    void set_device_id(uint8_t id) { device_id_ = static_cast<DeviceType>(id); }
    void set_brightness(uint8_t brightness) { brightness_ = brightness; }
    void set_servo_position(uint16_t position) { servo_position_ = position; }

    void set_servo(servo::Servo *servo) { servo_ = servo; }

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
    CoverStatus cover_status_ = COVER_CLOSED;
    ServoStatuses servo_status_ = MS_STOPPED;
    uint8_t light_status_ = 0;
    static const char *firmware_version_;

    servo::Servo *servo_ = nullptr;

    static constexpr float SERVO_POSITION_OPEN = 1.0f;
    static constexpr float SERVO_POSITION_CLOSED = -1.0f;
};

} // namespace snapcap
} // namespace esphome
