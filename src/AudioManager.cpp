#include "kyty/AudioManager.h"

namespace kyty::svc {

Result<void> AudioManager::Open(AudioConfig cfg) {
  if (cfg.sample_rate == 0 || cfg.channels == 0 || cfg.frames_per_buffer == 0)
    return Result<void>::Fail(RuntimeError::InvalidArgument, "bad audio cfg");
  cfg_ = cfg;
  state_ = AudioState::Open;
  queue_.clear();
  position_ = 0;
  return Result<void>::Ok();
}

Result<void> AudioManager::Start() {
  if (state_ != AudioState::Open && state_ != AudioState::Underrun)
    return Result<void>::Fail(RuntimeError::InvalidArgument,
                              "open first");
  state_ = AudioState::Running;
  last_poll_ns_ = clock_ ? clock_->MonotonicNs() : 0;
  return Result<void>::Ok();
}

Result<void> AudioManager::Stop() {
  state_ = AudioState::Open;
  return Result<void>::Ok();
}

Result<void> AudioManager::QueueBuffer(const std::vector<int16_t>& pcm) {
  if (state_ == AudioState::Closed)
    return Result<void>::Fail(RuntimeError::InvalidArgument, "not open");
  if (pcm.size() % cfg_.channels != 0)
    return Result<void>::Fail(RuntimeError::InvalidArgument,
                              "frame/channel mismatch");
  queue_.push_back(pcm);
  if (state_ == AudioState::Underrun) state_ = AudioState::Running;
  return Result<void>::Ok();
}

Result<uint64_t> AudioManager::Poll() {
  if (state_ != AudioState::Running)
    return Result<uint64_t>::Fail(RuntimeError::InvalidArgument,
                                  "not running");
  uint64_t now = clock_ ? clock_->MonotonicNs() : last_poll_ns_;
  uint64_t elapsed = now >= last_poll_ns_ ? now - last_poll_ns_ : 0;
  last_poll_ns_ = now;
  // Due frames = elapsed * rate / 1e9.
  uint64_t due_frames =
      elapsed * cfg_.sample_rate / 1'000'000'000ull;
  uint64_t consumed = 0;
  while (consumed < due_frames && !queue_.empty()) {
    auto& front = queue_.front();
    uint64_t front_frames = front.size() / cfg_.channels;
    if (consumed + front_frames <= due_frames) {
      consumed += front_frames;
      queue_.pop_front();
    } else {
      break;  // partial buffer stays (no tearing)
    }
  }
  position_ += consumed;
  if (queue_.empty() && due_frames > 0) state_ = AudioState::Underrun;
  return Result<uint64_t>::Ok(position_);
}

}  // namespace kyty::svc
