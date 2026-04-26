#include "snapcap.h"
#include "esphome/core/log.h"
#include "esphome/components/servo/servo.h"

#include <cstdio>

using esphome::tcp_server::TCPServerComponent;

namespace esphome {
namespace snapcap {

static const char *const TAG = "snapcap";

static bool parse_3_digits(const std::string &command, size_t offset, uint16_t &value) {
  if (command.size() < offset + 3)
    return false;

  const char c0 = command[offset + 0];
  const char c1 = command[offset + 1];
  const char c2 = command[offset + 2];
  if (c0 < '0' || c0 > '9' || c1 < '0' || c1 > '9' || c2 < '0' || c2 > '9')
    return false;

  value = static_cast<uint16_t>((c0 - '0') * 100 + (c1 - '0') * 10 + (c2 - '0'));
  return true;
}

const char *SnapCapComponent::firmware_version_ = "302";

#define LOG_SERVO(obj) \
  if ((obj) != nullptr) { \
    obj->dump_config(); \
  }

void SnapCapComponent::dump_config() {
  this->TCPServerComponent::dump_tcp_server_config_(TAG);
  ESP_LOGCONFIG(TAG, "SnapCap device ID: %d", device_id_);
  ESP_LOGCONFIG(TAG, "Brightness: %d", brightness_);
  ESP_LOGCONFIG(TAG, "Servo position: %d", servo_position_);
  ESP_LOGCONFIG(TAG, "Firmware version: %s", firmware_version_);
  LOG_SERVO(servo_);
}

void SnapCapComponent::setup(){
  ESP_LOGD(TAG, "SnapCap version %s", firmware_version_);
  // Call parent setup for proper initialization
  TCPServerComponent::setup();
  // Add SnapCap-specific setup logic here if needed
  if (servo_ != nullptr) {
      ESP_LOGD(TAG, "Scheduling initial servo position: %f", SERVO_POSITION_CLOSED);
      this->set_timeout(0, [this]() {
          if (this->servo_ != nullptr) {
              this->servo_->write(SERVO_POSITION_CLOSED);
          }
      });
  } else {
      ESP_LOGW(TAG, "No servo configured for SnapCapComponent");
  }
}

void SnapCapComponent::process_command(const std::string &command) {
    // Use a static buffer for all responses to minimize stack usage
    static char buf[32];
    std::string response;
    auto send_err = [this]() {
        this->send_response("*ERR\r\n");
    };

    if (command.size() < 2 || command[0] != '>') {
        send_err();
        return;
    }
    const char opcode = command[1];
    if (opcode == 'O') {
        // Open (small steps)
        if (servo_ == nullptr) {
            ESP_LOGW(TAG, "Received >O command but no servo is configured");
            send_err();
            return;
        }
        response = "*O000\r\n";
        cover_status_ = COVER_OPEN;
        servo_status_ = MS_RUNNING;
        servo_->write(SERVO_POSITION_OPEN);
    } else if (opcode == 'o') {
        // Force open (one step)
        if (servo_ == nullptr) {
            ESP_LOGW(TAG, "Received >o command but no servo is configured");
            send_err();
            return;
        }
        response = "*o000\r\n";
        cover_status_ = COVER_OPEN;
        servo_status_ = MS_RUNNING;
        servo_->write(SERVO_POSITION_OPEN);
    } else if (opcode == 'C') {
        // Close (small steps)
        if (servo_ == nullptr) {
            ESP_LOGW(TAG, "Received >C command but no servo is configured");
            send_err();
            return;
        }
        response = "*C000\r\n";
        cover_status_ = COVER_CLOSED;
        servo_status_ = MS_RUNNING;
        servo_->write(SERVO_POSITION_CLOSED);
    } else if (opcode == 'c') {
        // Force close (one step)
        if (servo_ == nullptr) {
            ESP_LOGW(TAG, "Received >c command but no servo is configured");
            send_err();
            return;
        }
        response = "*c000\r\n";
        cover_status_ = COVER_CLOSED;
        servo_status_ = MS_RUNNING;
        servo_->write(SERVO_POSITION_CLOSED);
    } else if (opcode == 'P') {
        // Ping response and state in one buffer
        snprintf(buf, sizeof(buf), "*P%02d00\r\n", device_id_);
        response = buf;
    } else if (opcode == 'A') {
        // Abort command
        if (servo_ == nullptr) {
            ESP_LOGW(TAG, "Received >A command but no servo is configured");
            send_err();
            return;
        }
        response = "*A000\r\n";
        servo_->detach();
        cover_status_ = COVER_USER_ABORT;
        servo_status_ = MS_STOPPED;
    } else if (opcode == 'B') {
        // Set brightness
        uint16_t val = 0;
        if (!parse_3_digits(command, 2, val) || val > 255) {
            send_err();
            return;
        }
        brightness_ = static_cast<uint8_t>(val);
        snprintf(buf, sizeof(buf), "*B%03d\r\n", brightness_);
        response = buf;
    } else if (opcode == 'J') {
        // Get brightness
        snprintf(buf, sizeof(buf), "*J%03d\r\n", brightness_);
        response = buf;
    } else if (opcode == 'L') {
        // Light on
        light_on_ = true;
        light_status_ = 1;
        response = "*L000\r\n";
    } else if (opcode == 'D') {
        // Light off
        light_on_ = false;
        light_status_ = 0;
        response = "*D000\r\n";
    } else if (opcode == 'V') {
        // Firmware version
        snprintf(buf, sizeof(buf), "*V%s\r\n", firmware_version_);
        response = buf;
    } else if (opcode == 'M') {
        // Get servo position
        snprintf(buf, sizeof(buf), "*M%03d\r\n", servo_position_);
        response = buf;
    } else if (opcode == 'N') {
        // Move servo position
        uint16_t pos = 0;
        if (!parse_3_digits(command, 2, pos)) {
            send_err();
            return;
        }
        if (servo_ == nullptr) {
            ESP_LOGW(TAG, "Received >N command but no servo is configured");
            send_err();
            return;
        }
        servo_position_ = pos;
        servo_status_ = MS_RUNNING;
        servo_->write(pos);
        snprintf(buf, sizeof(buf), "*N%03d\r\n", servo_position_);
        response = buf;
    } else if (opcode == 'S') {
        // Alternate wifi/serial
        if (servo_ == nullptr) {
            ESP_LOGW(TAG, "Received >S command but no servo is configured");
            send_err();
            return;
        }
        servo_status_ = servo_->has_reached_target() ? MS_STOPPED : MS_RUNNING;
        snprintf(buf, sizeof(buf), "*S%d%d%d\r\n", servo_status_, light_status_, cover_status_);
        response = buf;
    } else if (opcode == 'W') {
        // Alternate wifi/serial
        response = "*W000\r\n";
    } else {
        send_err();
        return;
    }
    this->send_response(response);
}

} // namespace snapcap
} // namespace esphome
