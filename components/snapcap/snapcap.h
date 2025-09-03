#pragma once
#include "esphome/components/tcp_server/tcp_server.h"

namespace esphome {
namespace snapcap {

class SnapCapComponent : public tcp_server::TCPServerComponent {
public:
    void process_command(const std::string &command) override;
    void dump_config() override;

    // Setters for configuration
    void set_device_id(uint8_t id) { device_id_ = id; }
    void set_brightness(uint8_t brightness) { brightness_ = brightness; }
    void set_servo_position(uint16_t position) { servo_position_ = position; }

protected:
    // Device state variables
    uint8_t device_id_ = 1;
    uint8_t brightness_ = 128;
    bool light_on_ = false;
    uint16_t servo_position_ = 0;
    uint8_t cover_status_ = COVER_CLOSED;
    uint8_t servo_status_ = 0;
    uint8_t light_status_ = 0;
    static const char *firmware_version_ = "302";

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
};

} // namespace snapcap
} // namespace esphome
