#include "esphome/components/tcp_server/ring_buffer.h"

#include "esphome/core/hal.h"

namespace esphome {
  namespace tcp_server {

    RingBuffer::RingBuffer(size_t size, const std::string &terminator)
        : size_(size < 2 ? 2 : size), buf_(new uint8_t[size_]), terminator_(terminator) {}

    bool RingBuffer::write(uint8_t byte) {
      if (free_space() == 0)
        return false;
      buf_[head_] = byte;
      head_ = index_(head_ + 1);
      last_write_time_ = ::esphome::millis();
      return true;
    }

    size_t RingBuffer::write_array(const uint8_t *data, size_t len) {
      size_t written = 0;
      for (size_t i = 0; i < len; i++) {
        if (!write(data[i]))
          break;
        written++;
      }
      return written;
    }

    std::string RingBuffer::read_line() {
        if (terminator_.empty())
            return "";

        const size_t avail = available();
        if (avail < terminator_.size())
            return "";

        const size_t last_start = avail - terminator_.size();
        for (size_t offset = 0; offset <= last_start; ++offset) {
            bool match = true;
            for (size_t i = 0; i < terminator_.size(); ++i) {
                if (buf_[index_(tail_ + offset + i)] != static_cast<uint8_t>(terminator_[i])) {
                    match = false;
                    break;
                }
            }

            if (!match)
                continue;

            const size_t line_len = offset + terminator_.size();
            std::string line(line_len, '\0');
            for (size_t i = 0; i < line_len; ++i)
                line[i] = static_cast<char>(buf_[index_(tail_ + i)]);

            tail_ = index_(tail_ + line_len);
            return line;
        }

        return "";
    }

    std::string RingBuffer::read_partial() {
        std::string result;
        result.reserve(available());
        size_t pos = tail_;
        while (pos != head_) {
            result.push_back(static_cast<char>(buf_[index_(pos)]));
            pos = (pos + 1) % size_;
        }
        return result;
    }

    std::string RingBuffer::flush_if_idle(uint32_t now, uint32_t timeout_ms) {
        if ((now - last_write_time_) < timeout_ms || available() == 0)
            return "";

        std::string partial = read_partial();
        tail_ = head_;  // clear after read
        return partial;
    }

    RingBuffer::BufferSlice RingBuffer::next_write_chunk() {
        if (is_full())
            return {nullptr, 0};

        if (head_ >= tail_) {
            size_t contiguous = size_ - head_;
            if (tail_ == 0)
                contiguous -= 1;  // Keep one byte empty to distinguish full/empty.
            return {buf_.get() + head_, contiguous};
        }

        return {buf_.get() + head_, tail_ - head_ - 1};
    }

    void RingBuffer::advance_head(size_t n) {
        const size_t room = free_space();
        if (n > room)
            n = room;
        head_ = index_(head_ + n);
        last_write_time_ = ::esphome::millis();
    }

    size_t RingBuffer::available() const {
      if (head_ >= tail_)
        return head_ - tail_;
      return size_ - (tail_ - head_);
    }

    size_t RingBuffer::free_space() const {
      return (size_ - available()) - 1;  // Leave 1-byte gap to distinguish full/empty.
    }

    void RingBuffer::clear() {
      head_ = tail_ = 0;
    }

    uint32_t RingBuffer::last_write_time() const {
      return last_write_time_;
    }

    size_t RingBuffer::index_(size_t pos) const {
      return pos % size_;
    }

  }  // namespace tcp_server
}  // namespace esphome