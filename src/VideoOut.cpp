#include "kyty/VideoOut.h"

namespace kyty::svc {

Result<void> VideoOutManager::Open(int display, VideoConfig cfg) {
  if (cfg.width == 0 || cfg.height == 0 || cfg.fps == 0)
    return Result<void>::Fail(RuntimeError::InvalidArgument, "bad mode");
  auto& d = displays_[display];
  if (d.open)
    return Result<void>::Fail(RuntimeError::InvalidArgument,
                              "display already open");
  d.open = true;
  d.cfg = cfg;
  return Result<void>::Ok();
}

Result<void> VideoOutManager::Close(int display) {
  auto it = displays_.find(display);
  if (it == displays_.end() || !it->second.open)
    return Result<void>::Fail(RuntimeError::InvalidArgument, "not open");
  it->second.open = false;
  it->second.queue.clear();
  return Result<void>::Ok();
}

Result<FrameInfo> VideoOutManager::Flip(int display, uint64_t submit_id) {
  auto it = displays_.find(display);
  if (it == displays_.end() || !it->second.open)
    return Result<FrameInfo>::Fail(RuntimeError::InvalidArgument,
                                   "flip on closed display");
  Display& d = it->second;
  uint64_t now = clock_ ? clock_->MonotonicNs() : 0;
  uint64_t period = 1'000'000'000ull / d.cfg.fps;
  FrameInfo f{d.next_frame++, submit_id, now + period};
  d.queue.push_back(f);
  // Present immediately up to one queued frame per Flip (VBlank-paced).
  if (d.queue.size() >= 1) {
    FrameInfo p = d.queue.front();
    d.queue.pop_front();
    d.vblank++;
    d.last_present_ns = p.present_ns;
    return Result<FrameInfo>::Ok(p);
  }
  return Result<FrameInfo>::Ok(f);
}

uint64_t VideoOutManager::VBlankCount(int display) const {
  auto it = displays_.find(display);
  return it == displays_.end() ? 0 : it->second.vblank;
}

uint64_t VideoOutManager::LastPresentNs(int display) const {
  auto it = displays_.find(display);
  return it == displays_.end() ? 0 : it->second.last_present_ns;
}

void VideoOutManager::TickVBlank(int display) {
  auto it = displays_.find(display);
  if (it == displays_.end()) return;
  it->second.vblank++;
}

}  // namespace kyty::svc
