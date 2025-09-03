#include "snapcap.h"
#include "esphome/core/log.h"

namespace esphome {
namespace snapcap {

static const char *const TAG = "snapcap";

void SnapCapComponent::dump_config() {
    ESP_LOGCONFIG(TAG, "SnapCap device ID: %d", device_id_);
    ESP_LOGCONFIG(TAG, "Brightness: %d", brightness_);
    ESP_LOGCONFIG(TAG, "Servo position: %d", servo_position_);
    ESP_LOGCONFIG(TAG, "Firmware version: %s", firmware_version_.c_str());
}

void SnapCapComponent::process_command(const std::string &command) {
    // Use a static buffer for all responses to minimize stack usage
    static char buf[32];
    std::string response;
    if (command.rfind(">O000", 0) == 0) {
        // Open (small steps)
        response = "*O000\n";
        cover_status_ = 1; // open
        servo_status_ = 1; // running
    } else if (command.rfind(">o000", 0) == 0) {
        // Force open (one step)
        response = "*o000\n";
        cover_status_ = 1;
        servo_status_ = 1;
    } else if (command.rfind(">C000", 0) == 0) {
        // Close (small steps)
        response = "*C000\n";
        cover_status_ = 2; // closed
        servo_status_ = 1;
    } else if (command.rfind(">c000", 0) == 0) {
        // Force close (one step)
        response = "*c000\n";
        cover_status_ = 2;
        servo_status_ = 1;
    } else if (command.rfind(">P000", 0) == 0) {
        // Ping response and state in one buffer
        snprintf(buf, sizeof(buf), "*P%02d000\n*S%d%d%d\n", device_id_, servo_status_, light_status_, cover_status_);
        response = buf;
    } else if (command.rfind(">B", 0) == 0 && command.size() >= 5) {
        // Set brightness
        int val = std::stoi(command.substr(2, 3));
        brightness_ = val;
        snprintf(buf, sizeof(buf), "*B%d\n", brightness_);
        response = buf;
    } else if (command.rfind(">J000", 0) == 0) {
        // Get brightness
        snprintf(buf, sizeof(buf), "*B%d\n", brightness_);
        response = buf;
    } else if (command.rfind(">L000", 0) == 0) {
        // Light on
        light_on_ = true;
        light_status_ = 1;
        response = "*L000\n";
    } else if (command.rfind(">D000", 0) == 0) {
        // Light off
        light_on_ = false;
        light_status_ = 0;
        response = "*D000\n";
    } else if (command.rfind(">V000", 0) == 0) {
        // Firmware version
        snprintf(buf, sizeof(buf), "*V%s\n", firmware_version_.c_str());
        response = buf;
    } else if (command.rfind(">M000", 0) == 0) {
        // Get servo position
        snprintf(buf, sizeof(buf), "*M%d\n", servo_position_);
        response = buf;
    } else if (command.rfind(">N", 0) == 0 && command.size() >= 5) {
        // Move servo position
        int pos = std::stoi(command.substr(2, 3));
        servo_position_ = pos;
        snprintf(buf, sizeof(buf), "*N%d\n", servo_position_);
        response = buf;
    } else if (command.rfind(">W000", 0) == 0) {
        // Alternate wifi/serial
        response = "*W000\n";
    } else {
        response = "*ERR\n";
    }
    this->send_response(response);
}

} // namespace snapcap
} // namespace esphome
