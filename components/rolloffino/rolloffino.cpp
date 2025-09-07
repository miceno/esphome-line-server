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
using namespace esphome;

namespace esphome {
	namespace rolloffino {
static const char *const TAG = "rolloffino";
static const char *const VERSION = "V1.7-esp-wifimanager-magnet-DRV8871";

void RolloffinoComponent::loop() {
  this->accept();
  if (this->clients_.size() > 0){
      // TCP → buffer
      this->read();
      // buffer → processing
      this->flush_tcp_buffer();
      this->cleanup();
  }
  // Unified non-blocking motor steps
  this->handle_motor_();
}

void RolloffinoComponent::dump_config() {
  LOG_TCP_SERVER(TAG, "Rolloffino", this);
  ESP_LOGCONFIG(
    TAG,
    "  Duty cycle: %u%%\n" \
    "  Max duration: %us\n" \
    "  Opened sensor: %s\n" \
    "  Closed sensor: %s",
    this->duty_cycle_,
    this->max_duration_,
    this->opened_binary_sensor_ ? this->opened_binary_sensor_->get_object_id().c_str() : "None",
    this->closed_binary_sensor_ ? this->closed_binary_sensor_->get_object_id().c_str() : "None"
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
	else if (command == "(GET:LOCKED:0)"){
		ESP_LOGV(TAG, "Locked status");
		response = "(ACK:LOCKED:OFF)";
	}
	else if (command == "(GET:AUXSTATE:0)"){
		ESP_LOGV(TAG, "Aux state");
		response = "(ACK:AUXSTATE:OFF)";
	} else {
		ESP_LOGE(TAG, "Unknown command: %s", command.c_str());
		response = "(NAK:ERROR:" + command + ")";
	}

	this->send_response(response);
}

void RolloffinoComponent::motor_open_() {
    ESP_LOGI(TAG, "Opening motor");
    // Start non-blocking open sequence using PWM
    if (this->in1_pin_ != nullptr && this->in2_pin_ != nullptr) {
				analogWrite(this->in1_pin_->get_pin(), map(this->duty_cycle_, 0, 100, 0, 255));  // NOLINT
        this->in2_pin_->digital_write(false);

        this->motor_direction_ = MOTOR_OPEN;
        this->motor_active_ = true;
        this->motor_move_start_time_ = esphome::micros();
    }
}

void RolloffinoComponent::motor_close_() {
    ESP_LOGI(TAG, "Closing motor");
    // Start non-blocking close sequence using PWM
    if (this->in1_pin_ != nullptr && this->in2_pin_ != nullptr) {
        this->in1_pin_->digital_write(false);
				analogWrite(this->in2_pin_->get_pin(), map(this->duty_cycle_, 0, 100, 0, 255));  // NOLINT

        this->motor_direction_ = MOTOR_CLOSE;
        this->motor_active_ = true;
        this->motor_move_start_time_ = esphome::micros();
    }
}

void RolloffinoComponent::motor_abort_() {
	ESP_LOGD(TAG, "Stopping motor");

  this->motor_active_ = false;
  this->motor_direction_ = MOTOR_NONE;
	this->in2_pin_->digital_write(true);
	this->in1_pin_->digital_write(true);
}

void RolloffinoComponent::handle_motor_() {
  if (!this->motor_active_ || this->motor_direction_ == MOTOR_NONE)
    return;

  uint32_t now = esphome::micros();
  if (this->max_duration_ > 0 && (now - this->motor_move_start_time_ > this->max_duration_ * 1000000UL)) {
    this->motor_abort_();
    ESP_LOGW(TAG, "Motor movement aborted due to timeout");
    return;
  }
}

	}  // namespace rolloffino
}  // namespace esphome