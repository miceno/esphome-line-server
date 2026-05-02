#include "rolloffino_cover.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/components/cover/cover.h"

namespace esphome {
namespace rolloffino_cover {

static const char *const TAG = "rolloffino_cover";

cover::CoverTraits RolloffinoCoverComponent::get_traits() {
  cover::CoverTraits traits;
  traits.set_is_assumed_state(true);
  traits.set_supports_stop(true);
  traits.set_supports_position(false);
  traits.set_supports_tilt(false);
  traits.set_supports_toggle(false);
  return traits;
}

void RolloffinoCoverComponent::control(const cover::CoverCall &call) {
  if (call.get_stop()) {
    this->motor_abort_();
    return;
  }

  if (call.get_position().has_value()) {
    const bool open = *call.get_position() >= 0.5f;
    if (open) {
      if (this->is_opened_()) {
        ESP_LOGW(TAG, "Ignoring OPEN command: opened limit sensor is active");
      } else {
        this->motor_open_();
      }
    } else {
      if (this->is_closed_()) {
        ESP_LOGW(TAG, "Ignoring CLOSE command: closed limit sensor is active");
      } else {
        this->motor_close_();
      }
    }
  }
}

void RolloffinoCoverComponent::sync_cover_state_() {
  // Determine position from sensors when available
  if (this->is_opened_()) {
    this->position = cover::COVER_OPEN;
  } else if (this->is_closed_()) {
    this->position = cover::COVER_CLOSED;
  } else if (this->motor_active_) {
    // moving, set operation based on direction
    this->position = cover::COVER_CLOSED;  // no intermediate position available
  } else {
    // unknown, use closed as default
    this->position = cover::COVER_CLOSED;
  }

  if (this->motor_active_) {
    this->current_operation = (this->motor_direction_ == MOTOR_OPEN)
                                  ? cover::COVER_OPERATION_OPENING
                                  : cover::COVER_OPERATION_CLOSING;
  } else {
    this->current_operation = cover::COVER_OPERATION_IDLE;
  }
}

void RolloffinoCoverComponent::publish_assumed_entities_() {
  this->sync_cover_state_();
  this->publish_state();
#ifdef USE_BINARY_SENSOR
  if (this->assumed_open_sensor_ != nullptr) {
    this->assumed_open_sensor_->publish_state(this->is_opened_());
  }
#endif
}

void RolloffinoCoverComponent::setup() {
  char tag_buf[36];
  snprintf(tag_buf, sizeof(tag_buf), "rolloffino_cover:%u", this->port_);
  this->set_log_tag(tag_buf);

  // Call parent TCPServer setup to init tcp server
  TCPServerComponent::setup();

  // Publish initial state
  this->publish_assumed_entities_();
}

void RolloffinoCoverComponent::loop() {
  // keep tcp_server behaviour
  TCPServerComponent::loop();
  // Unified non-blocking motor steps
  this->handle_motor_();
  // publish assumed entities each loop (small overhead)
  this->publish_assumed_entities_();
}

void RolloffinoCoverComponent::dump_config() {
  this->dump_tcp_server_config_(TAG);
  ESP_LOGCONFIG(TAG, "Rolloffino Cover config:");
  // print rolloffino-like info
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

void RolloffinoCoverComponent::process_command(const std::string &command) {
  ESP_LOGD(TAG, "Command is %s", command.c_str());

  std::string response;
  // Process command here (mirror rolloffino behaviour)
  if (command == "(CON:0:0)") {
    ESP_LOGD(TAG, "Connection request");
    response = "(ACK:0:0)";
  } else if (command == "(GET:OPENED:0)") {
    ESP_LOGD(TAG, "Opened status");
    response = this->is_opened_() ? "(ACK:OPENED:ON)" : "(ACK:OPENED:OFF)";
  } else if (command == "(GET:CLOSED:0)") {
    ESP_LOGD(TAG, "Closed status");
    response = this->is_closed_() ? "(ACK:CLOSED:ON)" : "(ACK:CLOSED:OFF)";
  } else if (command == "(SET:OPEN:ON)") {
    ESP_LOGD(TAG, "Open cover");
    response = "(ACK:OPEN:ON)";
    if (this->is_opened_()) {
      ESP_LOGW(TAG, "Ignoring OPEN command: opened limit sensor is active");
    } else {
      this->motor_open_();
    }
  } else if (command == "(SET:CLOSE:ON)") {
    ESP_LOGD(TAG, "Close cover");
    response = "(ACK:CLOSE:ON)";
    if (this->is_closed_()) {
      ESP_LOGW(TAG, "Ignoring CLOSE command: closed limit sensor is active");
    } else {
      this->motor_close_();
    }
  } else if (command == "(SET:ABORT:ON)") {
    ESP_LOGD(TAG, "Abort");
    response = "(ACK:ABORT:ON)";
    this->motor_abort_();
  } else if (command == "(GET:LOCKED:0)") {
    ESP_LOGD(TAG, "Locked status");
    response = "(ACK:LOCKED:OFF)";
  } else if (command == "(GET:AUXSTATE:0)") {
    ESP_LOGD(TAG, "Aux state");
    response = "(ACK:AUXSTATE:OFF)";
  } else {
    ESP_LOGE(TAG, "Unknown command: %s", command.c_str());
    response = "(NAK:ERROR:Unknown command)";
  }

  this->send_response(response);

  // after processing, publish state to reflect any motor changes
  this->publish_assumed_entities_();
}

// --- Motor implementation (copied/minimally adapted from rolloffino) ---
void RolloffinoCoverComponent::motor_open_() { this->motor_start_(MOTOR_OPEN, true, false); }

void RolloffinoCoverComponent::motor_close_() { this->motor_start_(MOTOR_CLOSE, false, true); }

void RolloffinoCoverComponent::motor_abort_() {
  ESP_LOGI(TAG, "Stopping motor");

  this->motor_active_ = false;
  this->motor_direction_ = MOTOR_NONE;
  if (this->in1_pin_)
    this->in1_pin_->digital_write(true);
  if (this->in2_pin_)
    this->in2_pin_->digital_write(true);
}

void RolloffinoCoverComponent::handle_motor_() {
  if (!this->motor_active_ || this->motor_direction_ == MOTOR_NONE)
    return;

  this->check_and_abort_on_limit_();

  uint32_t now = esphome::micros();
  const uint64_t elapsed_us = static_cast<uint32_t>(now - this->motor_move_start_time_);
  const uint64_t timeout_us = static_cast<uint64_t>(this->max_duration_) * 1000000ULL;
  if (this->max_duration_ > 0 && elapsed_us > timeout_us) {
    this->motor_abort_();
    ESP_LOGW(TAG, "Motor movement aborted due to timeout");
    return;
  }
}

bool RolloffinoCoverComponent::is_opened_() const {
  return this->opened_binary_sensor_ != nullptr && this->opened_binary_sensor_->state;
}

bool RolloffinoCoverComponent::is_closed_() const {
  return this->closed_binary_sensor_ != nullptr && this->closed_binary_sensor_->state;
}

void RolloffinoCoverComponent::motor_start_(MotorDirection direction, bool in1_state, bool in2_state) {
  const char *direction_str = (direction == MOTOR_OPEN) ? "OPEN" : "CLOSE";
  ESP_LOGI(TAG, "Starting motor: %s", direction_str);
  this->motor_direction_ = direction;
  this->motor_active_ = true;
  this->motor_move_start_time_ = esphome::micros();
  if (this->in1_pin_)
    this->in1_pin_->digital_write(in1_state);
  if (this->in2_pin_)
    this->in2_pin_->digital_write(in2_state);
}

void RolloffinoCoverComponent::check_and_abort_on_limit_() {
  if (this->motor_direction_ == MOTOR_OPEN && this->is_opened_()) {
    this->motor_abort_();
    ESP_LOGW(TAG, "Motor movement aborted: opened limit sensor reached");
    return;
  }
  if (this->motor_direction_ == MOTOR_CLOSE && this->is_closed_()) {
    this->motor_abort_();
    ESP_LOGW(TAG, "Motor movement aborted: closed limit sensor reached");
    return;
  }
}

}  // namespace rolloffino_cover
}  // namespace esphome



