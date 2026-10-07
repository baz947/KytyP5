#pragma once
// VideoOut state machine — README §10.
// Guest VideoOut -> Manager -> Frame Scheduler -> GPU Timeline -> Present.
// Tracks display/output/surface/flip/VBlank/VRR/frame queue/HDR/timing.
#include <cstdint>
#include <deque>
#include <string>
#include <unordered_map>
#include "kyty/Compat.h"
#include "kyty/GuestClock.h"

namespace kyty::svc {

struct VideoConfig {
  uint32_t width = 1920;
  uint32_t height = 1080;
  uint32_t fps = 60;
  bool hdr = false;
  bool vrr = false;
};

struct FrameInfo {
  uint64_t frame_id = 0;
  uint64_t submit_id = 0;
  uint64_t present_ns = 0;
};

class VideoOutManager {
 public:
  explicit VideoOutManager(GuestClock* clock) : clock_(clock) {}

  Result<void> Open(int display, VideoConfig cfg);
  Result<void> Close(int display);
  // Enqueue a finished GPU submit for presentation on next VBlank.
  Result<FrameInfo> Flip(int display, uint64_t submit_id);
  [[nodiscard]] uint64_t VBlankCount(int display) const;
  [[nodiscard]] uint64_t LastPresentNs(int display) const;
  // Deterministic VBlank advance for tests/chaos.
  void TickVBlank(int display);

 private:
  struct Display {
    bool open = false;
    VideoConfig cfg;
    uint64_t vblank = 0;
    uint64_t next_frame = 1;
    uint64_t last_present_ns = 0;
    std::deque<FrameInfo> queue;
  };
  GuestClock* clock_ = nullptr;
  std::unordered_map<int, Display> displays_;
};

}  // namespace kyty::svc
