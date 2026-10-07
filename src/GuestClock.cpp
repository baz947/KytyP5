#include "kyty/GuestClock.h"

namespace kyty {

GuestClock::GuestClock() : boot_(std::chrono::steady_clock::now()) {}

uint64_t GuestClock::RealNs() const {
  auto now =
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count();
  return uint64_t(now) + injected_ns_;
}

uint64_t GuestClock::MonotonicNs() const {
  auto d = std::chrono::steady_clock::now() - boot_;
  return uint64_t(
             std::chrono::duration_cast<std::chrono::nanoseconds>(d)
                 .count()) +
         injected_ns_;
}

uint64_t GuestClock::ProcessNs() const { return MonotonicNs(); }

uint64_t GuestClock::ThreadNs(uint64_t) const { return MonotonicNs(); }

uint64_t GuestClock::GuestTicks() const { return ticks_ + MonotonicNs(); }

void GuestClock::AdvanceNs(uint64_t delta) {
  injected_ns_ += delta;
  ticks_ += delta;  // 1 tick == 1 ns
}

}  // namespace kyty
