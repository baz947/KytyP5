#include <cassert>
#include <cstdio>
#include <cstring>
#include "kyty/GuestMemory.h"

int main() {
  using namespace kyty;
  GuestMemory mem;
  // page-aligned map
  assert(mem.Map(0x10000, 0x4000 * 2, PageState::ReadWrite).ok());
  // write/read roundtrip
  const char* msg = "kyty";
  assert(mem.Write(0x10000, msg, 5).ok());
  char buf[5] = {};
  assert(mem.Read(0x10000, buf, 5).ok());
  assert(std::memcmp(buf, msg, 5) == 0);

  // cross-page op: spans 2 pages
  assert(mem.Write(0x10000 + 0x4000 - 2, "ABCD", 4).ok());
  char cross[4] = {};
  assert(mem.Read(0x10000 + 0x4000 - 2, cross, 4).ok());
  assert(std::memcmp(cross, "ABCD", 4) == 0);

  // unmapped -> InvalidGuestMemory (not return 0)
  char tmp[4];
  auto r = mem.Read(0x90000, tmp, 4);
  assert(!r.ok() && r.error == RuntimeError::InvalidGuestMemory);

  // protect to read-only -> write must fail
  assert(mem.Protect(0x10000, 0x4000, uint8_t(Access::Read)).ok());
  assert(!mem.Write(0x10000, msg, 5).ok());
  // range guards: zero size / out-of-range never silently Ok
  assert(!mem.Protect(0x10000, 0, uint8_t(Access::Read)).ok());
  assert(!mem.Synchronize(0x10000, 0).ok());

  // BDA bounded retry: resolver never makes progress -> controlled failure
  auto bda = mem.ResolveBda(0x90000, 8, [](uint32_t) { return false; });
  assert(!bda.ok() && bda.error == RuntimeError::InvalidGuestMemory);

  // BDA retry with progress on 2nd attempt
  int calls = 0;
  GuestMemory mem2;
  auto bda2 = mem2.ResolveBda(0x20000, 8, [&](uint32_t) {
    ++calls;
    if (calls == 2) mem2.Map(0x20000, 0x4000, PageState::ReadWrite);
    return true;
  });
  assert(bda2.ok());

  // versioning: write bumps cpu version, sync bumps gpu version
  uint64_t v0 = mem2.CpuVersion(), g0 = mem2.GpuVersion();
  mem2.Write(0x20000, "x", 1);
  mem2.Synchronize(0x20000, 8);
  assert(mem2.CpuVersion() > v0 && mem2.GpuVersion() > g0);

  std::puts("test_memory OK");
  return 0;
}
