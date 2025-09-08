#include "snapcap.h"
#include "esphome/core/log.h"
#include "esphome/components/servo/servo.h"

using esphome::tcp_server::TCPServerComponent;

namespace esphome {
namespace snapcap {

static const char *const TAG = "snapcap";

const char *SnapCapComponent::firmware_version_ = "302";

#define LOG_SERVO(obj) \
  if ((obj) != nullptr) { \
    obj->dump_config(); \
  }

void SnapCapComponent::dump_config() {
  LOG_TCP_SERVER(TAG, "SnapCap", this);
  ESP_LOGCONFIG(TAG, "SnapCap device ID: %d", device_id_);
  ESP_LOGCONFIG(TAG, "Brightness: %d", brightness_);
  ESP_LOGCONFIG(TAG, "Servo position: %d", servo_position_);
  ESP_LOGCONFIG(TAG, "Firmware version: %s", firmware_version_);
  LOG_SERVO(servo_);
}

void SnapCapComponent::setup(){
  ESP_LOGD(TAG, "SnapCap version %s", firmware_version_);}
  // Call parent setup for proper initialization
  TCPServerComponent::setup();
  // Add SnapCap-specific setup logic here if needed
  if (servo_ != nullptr) {
      ESP_LOGD(TAG, "Initial servo position: %f", SERVO_POSITION_CLOSED);
      servo_->setup();
      servo_->write(SERVO_POSITION_CLOSED); // Initial position
  } else {
      ESP_LOGW(TAG, "No servo configured for SnapCapComponent");
  }
}

void SnapCapComponent::process_command(const std::string &command) {
    // Use a static buffer for all responses to minimize stack usage
    static char buf[32];
    std::string response;
    const char *command_str = command.c_str();
    if (command.empty() || command[0] != '>') {
        response = "*ERR\r\n";
        this->send_response(response);
        return;
    }
    if (command_str[1] == 'O') {
        // Open (small steps)
        response = "*O000\r\n";
        cover_status_ = COVER_OPEN;
        servo_status_ = MS_RUNNING;
        servo_->write(SERVO_POSITION_OPEN);
    } else if (command_str[1] == 'o') {
        // Force open (one step)
        response = "*o000\r\n";
        cover_status_ = COVER_OPEN;
        servo_status_ = MS_RUNNING;
        servo_->write(SERVO_POSITION_OPEN);
    } else if (command_str[1] == 'C') {
        // Close (small steps)
        response = "*C000\r\n";
        cover_status_ = COVER_CLOSED;
        servo_status_ = MS_RUNNING;
        servo_->write(SERVO_POSITION_CLOSED);
    } else if (command_str[1] == 'c') {
        // Force close (one step)
        response = "*c000\r\n";
        cover_status_ = COVER_CLOSED;
        servo_status_ = MS_RUNNING;
        servo_->write(SERVO_POSITION_CLOSED);
    } else if (command_str[1] == 'P') {
        // Ping response and state in one buffer
        snprintf(buf, sizeof(buf), "*P%02d00\r\n", device_id_);
        response = buf;
    } else if (command_str[1] == 'A') {
        // Abort command
        response = "*A000\r\n";
        servo_->detach();
        cover_status_ = COVER_USER_ABORT;
        servo_status_ = MS_STOPPED;
    } else if (command_str[1] == 'B' && command.size() >= 5) {
        // Set brightness
        int val = std::stoi(command.substr(2, 3));
        brightness_ = val;
        snprintf(buf, sizeof(buf), "*B%03d\r\n", brightness_);
        response = buf;
    } else if (command_str[1] == 'J') {
        // Get brightness
        snprintf(buf, sizeof(buf), "*J%03d\r\n", brightness_);
        response = buf;
    } else if (command_str[1] == 'L') {
        // Light on
        light_on_ = true;
        light_status_ = 1;
        response = "*L000\r\n";
    } else if (command_str[1] == 'D') {
        // Light off
        light_on_ = false;
        light_status_ = 0;
        response = "*D000\r\n";
    } else if (command_str[1] == 'V') {
        // Firmware version
        snprintf(buf, sizeof(buf), "*V%s\r\n", firmware_version_);
        response = buf;
    } else if (command_str[1] == 'M') {
        // Get servo position
        snprintf(buf, sizeof(buf), "*M%03d\r\n", servo_position_);
        response = buf;
    } else if (command_str[1] == 'N' && command.size() >= 5) {
        // Move servo position
        int pos = std::stoi(command.substr(2, 3));
        servo_position_ = pos;
        snprintf(buf, sizeof(buf), "*N%03d\r\n", servo_position_);
        response = buf;
    } else if (command_str[1] == 'S') {
        // Alternate wifi/serial
        servo_status_ = servo_->has_reached_target() ? MS_STOPPED : MS_RUNNING;
        snprintf(buf, sizeof(buf), "*S%d%d%d\r\n", servo_status_, light_status_, cover_status_);
        response = buf;
    } else if (command_str[1] == 'W') {
        // Alternate wifi/serial
        response = "*W000\r\n";
    } else {
        response = "*ERR\r\n";
    }
    this->send_response(response);
}

} // namespace snapcap
} // namespace esphome
