#include "snapcap_cover.h"

#include <cstdio>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

using esphome::tcp_server::TCPServerComponent;

namespace esphome {
namespace snapcap_cover {

static const char *const TAG = "snapcap_cover";
const char *SnapCapCoverComponent::firmware_version_ = "100";

cover::CoverTraits SnapCapCoverComponent::get_traits() {
  cover::CoverTraits traits;
  traits.set_is_assumed_state(true);
  traits.set_supports_stop(true);
  traits.set_supports_position(false);
  traits.set_supports_tilt(false);
  traits.set_supports_toggle(false);
  return traits;
}

void SnapCapCoverComponent::control(const cover::CoverCall &call) {
  if (call.get_stop()) {
    this->abort_motion_();
    return;
  }

  if (call.get_position().has_value()) {
    this->begin_motion_(*call.get_position() >= 0.5f);
  }
}

void SnapCapCoverComponent::sync_cover_state_() {
  this->position = this->current_opened_ ? cover::COVER_OPEN : cover::COVER_CLOSED;
  if (this->cover_status_ == COVER_MOVING) {
    this->current_operation = this->target_opened_ ? cover::COVER_OPERATION_OPENING : cover::COVER_OPERATION_CLOSING;
  } else {
    this->current_operation = cover::COVER_OPERATION_IDLE;
  }
}

void SnapCapCoverComponent::publish_assumed_entities_() {
  this->sync_cover_state_();
  this->publish_state();
#ifdef USE_BINARY_SENSOR
  if (this->assumed_open_sensor_ != nullptr) {
    this->assumed_open_sensor_->publish_state(this->current_opened_);
  }
#endif
#ifdef USE_TEXT_SENSOR
  if (this->state_text_sensor_ != nullptr) {
    const char *state = "closed";
    if (this->cover_status_ == COVER_MOVING) {
      state = "moving";
    } else if (this->cover_status_ == COVER_OPEN) {
      state = "open";
    } else if (this->cover_status_ == COVER_USER_ABORT) {
      state = "aborted";
    }
    this->state_text_sensor_->publish_state(state);
  }
#endif
}

void SnapCapCoverComponent::setup() {
  char tag_buf[36];
  snprintf(tag_buf, sizeof(tag_buf), "snapcap_cover:%u", this->port_);
  this->set_log_tag(tag_buf);

  this->target_opened_ = this->initial_opened_;
  this->current_opened_ = this->initial_opened_;
  this->cover_status_ = this->initial_opened_ ? COVER_OPEN : COVER_CLOSED;
  this->servo_status_ = MS_STOPPED;
  this->publish_assumed_entities_();

  TCPServerComponent::setup();

  if (this->servo_ != nullptr) {
    const float level = this->initial_opened_ ? this->open_level_ : this->closed_level_;
    this->set_timeout(0, [this, level]() { this->servo_->write(level); });
  } else {
    ESP_LOGW(this->log_tag_.c_str(), "No servo configured for SnapCapCoverComponent");
  }
}

void SnapCapCoverComponent::loop() {
  TCPServerComponent::loop();
  this->update_motion_state_();
}

void SnapCapCoverComponent::dump_config() {
  this->dump_tcp_server_config_(TAG);
  ESP_LOGCONFIG(TAG, "SnapCap Cover device ID: %u", this->device_id_);
  ESP_LOGCONFIG(TAG, "Open level: %.2f", this->open_level_);
  ESP_LOGCONFIG(TAG, "Closed level: %.2f", this->closed_level_);
  ESP_LOGCONFIG(TAG, "Move duration: %ums", this->move_duration_ms_);
  ESP_LOGCONFIG(TAG, "Initial state: %s", this->initial_opened_ ? "OPEN" : "CLOSED");
  if (this->servo_ != nullptr) {
    this->servo_->dump_config();
  }
}

void SnapCapCoverComponent::begin_motion_(bool open) {
  if (this->servo_ == nullptr) {
    return;
  }

  this->target_opened_ = open;
  this->motion_started_at_ = esphome::millis();
  this->servo_status_ = MS_RUNNING;
  this->cover_status_ = COVER_MOVING;
  this->publish_assumed_entities_();

  this->servo_->write(open ? this->open_level_ : this->closed_level_);
}

void SnapCapCoverComponent::abort_motion_() {
  if (this->servo_ != nullptr) {
    this->servo_->detach();
  }
  this->servo_status_ = MS_STOPPED;
  this->cover_status_ = COVER_USER_ABORT;
  this->publish_assumed_entities_();
}

void SnapCapCoverComponent::update_motion_state_() {
  if (this->servo_status_ != MS_RUNNING) {
    return;
  }

  const uint32_t elapsed_ms = esphome::millis() - this->motion_started_at_;
  if (elapsed_ms < this->move_duration_ms_) {
    return;
  }

  this->servo_status_ = MS_STOPPED;
  this->current_opened_ = this->target_opened_;
  this->cover_status_ = this->current_opened_ ? COVER_OPEN : COVER_CLOSED;
  this->publish_assumed_entities_();
}

void SnapCapCoverComponent::process_command(const std::string &command) {
  if (command.size() < 2 || command[0] != '>') {
    this->send_response("*ERR\r\n");
    return;
  }

  const char opcode = command[1];
  char buf[32];

  if (opcode == 'O') {
    if (this->servo_ == nullptr) {
      this->send_response("*ERR\r\n");
      return;
    }
    this->begin_motion_(true);
    this->send_response("*O000\r\n");
    return;
  }

  if (opcode == 'o') {
    if (this->servo_ == nullptr) {
      this->send_response("*ERR\r\n");
      return;
    }
    this->begin_motion_(true);
    this->send_response("*o000\r\n");
    return;
  }

  if (opcode == 'C') {
    if (this->servo_ == nullptr) {
      this->send_response("*ERR\r\n");
      return;
    }
    this->begin_motion_(false);
    this->send_response("*C000\r\n");
    return;
  }

  if (opcode == 'c') {
    if (this->servo_ == nullptr) {
      this->send_response("*ERR\r\n");
      return;
    }
    this->begin_motion_(false);
    this->send_response("*c000\r\n");
    return;
  }

  if (opcode == 'A') {
    if (this->servo_ == nullptr) {
      this->send_response("*ERR\r\n");
      return;
    }
    this->abort_motion_();
    this->send_response("*A000\r\n");
    return;
  }

  if (opcode == 'P') {
    snprintf(buf, sizeof(buf), "*P%02u00\r\n", this->device_id_);
    this->send_response(buf);
    return;
  }

  if (opcode == 'S') {
    snprintf(buf, sizeof(buf), "*S%d0%d\r\n", this->servo_status_, this->cover_status_);
    this->send_response(buf);
    return;
  }

  if (opcode == 'V') {
    snprintf(buf, sizeof(buf), "*V%s\r\n", firmware_version_);
    this->send_response(buf);
    return;
  }

  if (opcode == 'W') {
    this->send_response("*W000\r\n");
    return;
  }

  this->send_response("*ERR\r\n");
}

}  // namespace snapcap_cover
}  // namespace esphome


