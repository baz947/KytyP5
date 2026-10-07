#pragma once
// Audio — README §10. Shared Audio/Guest clock, buffer queue,
// sample position, latency and device state.
#include <cstdint>
#include <deque>
#include <vector>
#include "kyty/Compat.h"
#include "kyty/GuestClock.h"

namespace kyty::svc {

enum class AudioState : uint8_t { Closed = 0, Open, Running, Underrun };

struct AudioConfig {
  uint32_t sample_rate = 48000;
  uint32_t channels = 2;
  uint32_t frames_per_buffer = 512;
};

class AudioManager {
 public:
  explicit AudioManager(GuestClock* clock) : clock_(clock) {}
  Result<void> Open(AudioConfig cfg);
  Result<void> Start();
  Result<void> Stop();
  Result<void> QueueBuffer(const std::vector<int16_t>& pcm);
  // Consume due buffers according to guest clock; reports position.
  Result<uint64_t> Poll();  // returns sample position
  [[nodiscard]] AudioState State() const { return state_; }
  [[nodiscard]] uint64_t SamplePosition() const { return position_; }
  [[nodiscard]] size_t Queued() const { return queue_.size(); }

 private:
  GuestClock* clock_ = nullptr;
  AudioConfig cfg_;
  AudioState state_ = AudioState::Closed;
  std::deque<std::vector<int16_t>> queue_;
  uint64_t position_ = 0;
  uint64_t last_poll_ns_ = 0;
};

}  // namespace kyty::svc
