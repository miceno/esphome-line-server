#include "rolloffino.h"

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/util.h"
#include "esphome/core/version.h"
#include "esphome/components/network/util.h"
#include "esphome/components/socket/socket.h"
#include "esphome/components/socket/headers.h"
#include "../tcp_server/ring_buffer.h"

using esphome::tcp_server::RingBuffer;
using esphome::tcp_server::TCPServerComponent;
using namespace esphome;

namespace esphome {
    namespace rolloffino {
static const char *const TAG = "rolloffino";
static const char *const VERSION = "V1.7-esp-wifimanager-magnet-DRV8871";

void RolloffinoComponent::loop() {
  this->TCPServerComponent::loop();
  // Unified non-blocking motor steps
  this->handle_motor_();
}

void RolloffinoComponent::dump_config() {
  this->TCPServerComponent::dump_tcp_server_config_(TAG);
  std::array<char, 128> opened_sensor_obj_id{};
  std::array<char, 128> closed_sensor_obj_id{};

  const char *opened_sensor_name = "None";
  if (this->opened_binary_sensor_ != nullptr) {
    opened_sensor_name = this->opened_binary_sensor_->get_object_id_to(opened_sensor_obj_id).c_str();
  }

  const char *closed_sensor_name = "None";
  if (this->closed_binary_sensor_ != nullptr) {
    closed_sensor_name = this->closed_binary_sensor_->get_object_id_to(closed_sensor_obj_id).c_str();
  }

  ESP_LOGCONFIG(
    TAG,
    "  Duty cycle: %u%%\n" \
    "  Max duration: %us\n" \
    "  Opened sensor: %s\n" \
    "  Closed sensor: %s",
    this->duty_cycle_,
    this->max_duration_,
    opened_sensor_name,
    closed_sensor_name
  );
  LOG_PIN("  IN1 pin: ", this->in1_pin_);
  LOG_PIN("  IN2 pin: ", this->in2_pin_);
}


void RolloffinoComponent::process_command(const std::string &command){
    ESP_LOGD(TAG, "Command is %s", command.c_str());

    std::string response;
    // Process command here
    if( command == "(CON:0:0)" ){
        ESP_LOGV(TAG, "Connection request");
        response = "(ACK:0:0)";
    }
    else if (command == "(GET:OPENED:0)"){
        ESP_LOGV(TAG, "Opened status");
        response = "(ACK:OPENED:";
        if (this->opened_binary_sensor_ != nullptr && this->opened_binary_sensor_->state) {
            response += "ON)";
        } else {
            response += "OFF)";
        }
    }
    else if (command == "(GET:CLOSED:0)"){
        ESP_LOGV(TAG, "Closed status");
        response = "(ACK:CLOSED:";
        if (this->closed_binary_sensor_ != nullptr && this->closed_binary_sensor_->state) {
            response += "ON)";
        } else {
            response += "OFF)";
        }
    }
    else if (command == "(SET:OPEN:ON)"){
        ESP_LOGV(TAG, "Open cover");
        response = "(ACK:OPEN:ON)";
        this->motor_open_();
    }
    else if (command == "(SET:CLOSE:ON)"){
        ESP_LOGV(TAG, "Close cover");
        response = "(ACK:CLOSE:ON)";
        this->motor_close_();
    }
    else if (command == "(SET:ABORT:ON)"){
        ESP_LOGV(TAG, "Abort");
        response = "(ACK:ABORT:ON)";
        this->motor_abort_();
    }
    else if (command == "(GET:LOCKED:0)"){
        ESP_LOGV(TAG, "Locked status");
        response = "(ACK:LOCKED:OFF)";
    }
    else if (command == "(GET:AUXSTATE:0)"){
        ESP_LOGV(TAG, "Aux state");
        response = "(ACK:AUXSTATE:OFF)";
    } else {
        ESP_LOGE(TAG, "Unknown command: %s", command.c_str());
        response = "(NAK:ERROR:Unknown command)";
    }

    this->send_response(response);
}

void RolloffinoComponent::motor_open_() {
    ESP_LOGI(TAG, "Opening motor");
    // Start non-blocking open sequence using PWM
    this->motor_direction_ = MOTOR_OPEN;
    this->motor_active_ = true;
    this->motor_move_start_time_ = esphome::micros();

    // analogWrite(this->in1_pin_->get_pin(), map(this->duty_cycle_, 0, 100, 0, 255));  // NOLINT
    this->in1_pin_->digital_write(true);
    this->in2_pin_->digital_write(false);
}

void RolloffinoComponent::motor_close_() {
    ESP_LOGI(TAG, "Closing motor");
    // Start non-blocking close sequence using PWM
    this->motor_direction_ = MOTOR_CLOSE;
    this->motor_active_ = true;
    this->motor_move_start_time_ = esphome::micros();

    this->in1_pin_->digital_write(false);
    // analogWrite(this->in2_pin_->get_pin(), map(this->duty_cycle_, 0, 100, 0, 255));  // NOLINT
    this->in2_pin_->digital_write(true);
}

void RolloffinoComponent::motor_abort_() {
  ESP_LOGI(TAG, "Stopping motor");

  this->motor_active_ = false;
  this->motor_direction_ = MOTOR_NONE;
  this->in1_pin_->digital_write(true);
  this->in2_pin_->digital_write(true);
}

void RolloffinoComponent::handle_motor_() {
  if (!this->motor_active_ || this->motor_direction_ == MOTOR_NONE)
    return;

  uint32_t now = esphome::micros();
  const uint64_t elapsed_us = static_cast<uint32_t>(now - this->motor_move_start_time_);
  const uint64_t timeout_us = static_cast<uint64_t>(this->max_duration_) * 1000000ULL;
  if (this->max_duration_ > 0 && elapsed_us > timeout_us) {
    this->motor_abort_();
    ESP_LOGW(TAG, "Motor movement aborted due to timeout");
    return;
  }
}

void RolloffinoComponent::setup() {
  ESP_LOGD(TAG, "Rolloffino version %s", VERSION);
  // Call parent setup for proper initialization
  TCPServerComponent::setup();
  // Add Rolloffino-specific setup logic here if needed
  in1_pin_->setup();
  in2_pin_->setup();
  motor_abort_();
}

    }  // namespace rolloffino
}  // namespace esphome