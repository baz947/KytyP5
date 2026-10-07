#pragma once
// Guest Clock — README §4 (P0): one authoritative clock for
// real/monotonic/process/thread/guest-ticks shared by all subsystems.
#include <chrono>
#include <cstdint>

namespace kyty {

class GuestClock {
 public:
  static constexpr uint64_t kTicksPerSecond = 1'000'000'000;  // ns ticks

  GuestClock();

  [[nodiscard]] uint64_t RealNs() const;       // wall time
  [[nodiscard]] uint64_t MonotonicNs() const;  // steady time since boot
  [[nodiscard]] uint64_t ProcessNs() const;    // process time (== monotonic here)
  [[nodiscard]] uint64_t ThreadNs(uint64_t thread_id) const;
  [[nodiscard]] uint64_t GuestTicks() const;   // guest tick counter

  // Deterministic advance for tests / chaos mode.
  void AdvanceNs(uint64_t delta);
  void AdvanceTicks(uint64_t delta) { ticks_ += delta; }

 private:
  std::chrono::steady_clock::time_point boot_;
  uint64_t injected_ns_ = 0;
  uint64_t ticks_ = 0;
};

}  // namespace kyty
