#include <cassert>
#include <cstdio>
#include "kyty/GuestClock.h"

int main() {
  using namespace kyty;
  GuestClock c;
  uint64_t t0 = c.MonotonicNs();
  uint64_t g0 = c.GuestTicks();
  c.AdvanceNs(1000);
  assert(c.MonotonicNs() >= t0 + 1000);
  assert(c.GuestTicks() >= g0 + 1000);
  assert(c.ProcessNs() >= t0);
  assert(c.RealNs() > 0);
  std::puts("test_clock OK");
  return 0;
}
