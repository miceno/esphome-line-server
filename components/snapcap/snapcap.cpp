#include "snapcap.h"
#include "esphome/core/log.h"

namespace esphome {
namespace snapcap {

static const char *const TAG = "snapcap";

const char *SnapCapComponent::firmware_version_ = "302";

void SnapCapComponent::dump_config() {
    TCPServerComponent::dump_config();
    ESP_LOGCONFIG(TAG, "SnapCap device ID: %d", device_id_);
    ESP_LOGCONFIG(TAG, "Brightness: %d", brightness_);
    ESP_LOGCONFIG(TAG, "Servo position: %d", servo_position_);
    ESP_LOGCONFIG(TAG, "Firmware version: %s", firmware_version_);
}

void SnapCapComponent::process_command(const std::string &command) {
    // Use a static buffer for all responses to minimize stack usage
    static char buf[32];
    std::string response;
    const char *command_str = command.c_str();
    if (command.empty() || command[0] != '>') {
				response = "*ERR\n";
				this->send_response(response);
				return;
		}
    if (command_str[1] == 'O') {
        // Open (small steps)
        response = "*O000\n";
        cover_status_ = COVER_OPEN;
        servo_status_ = 1;
    } else if (command_str[1] == 'o') {
        // Force open (one step)
        response = "*o000\n";
        cover_status_ = COVER_OPEN;
        servo_status_ = 1;
    } else if (command_str[1] == 'C') {
        // Close (small steps)
        response = "*C000\n";
        cover_status_ = COVER_CLOSED;
        servo_status_ = 1;
    } else if (command_str[1] == 'c') {
        // Force close (one step)
        response = "*c000\n";
        cover_status_ = COVER_CLOSED;
        servo_status_ = 1;
    } else if (command_str[1] == 'P') {
        // Ping response and state in one buffer
        snprintf(buf, sizeof(buf), "*P%02d000\n*S%d%d%d\n", device_id_, servo_status_, light_status_, cover_status_);
        response = buf;
    } else if (command_str[1] == 'B' && command.size() >= 5) {
        // Set brightness
        int val = std::stoi(command.substr(2, 3));
        brightness_ = val;
        snprintf(buf, sizeof(buf), "*B%d\n", brightness_);
        response = buf;
    } else if (command_str[1] == 'J') {
        // Get brightness
        snprintf(buf, sizeof(buf), "*B%d\n", brightness_);
        response = buf;
    } else if (command_str[1] == 'L') {
        // Light on
        light_on_ = true;
        light_status_ = 1;
        response = "*L000\n";
    } else if (command_str[1] == 'D') {
        // Light off
        light_on_ = false;
        light_status_ = 0;
        response = "*D000\n";
    } else if (command_str[1] == 'V') {
        // Firmware version
        snprintf(buf, sizeof(buf), "*V%s\n", firmware_version_);
        response = buf;
    } else if (command_str[1] == 'M') {
        // Get servo position
        snprintf(buf, sizeof(buf), "*M%d\n", servo_position_);
        response = buf;
    } else if (command_str[1] == 'N' && command.size() >= 5) {
        // Move servo position
        int pos = std::stoi(command.substr(2, 3));
        servo_position_ = pos;
        snprintf(buf, sizeof(buf), "*N%d\n", servo_position_);
        response = buf;
    } else if (command_str[1] == 'S') {
        // Alternate wifi/serial
        snprintf(buf, sizeof(buf), "*S%d%d%d\n", servo_status_, light_status_, cover_status_);
				response = buf;
    } else if (command_str[1] == 'W') {
        // Alternate wifi/serial
        response = "*W000\n";
    } else {
        response = "*ERR\n";
    }
    this->send_response(response);
}

} // namespace snapcap
} // namespace esphome
