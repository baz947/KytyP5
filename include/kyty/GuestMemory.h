#pragma once
// Guest Memory — README §2 (P0)
// Separate GuestAddress / GuestPage / HostMapping / GpuAddress.
// Page states, versioning, cross-page aware ops, BDA bounded retry.
#include <cstdint>
#include <map>
#include <vector>
#include "kyty/Compat.h"

namespace kyty {

using GuestAddress = uint64_t;
using GpuAddress = uint64_t;

inline constexpr uint64_t kPageSize = 0x4000;  // PS5: 16 KiB
inline constexpr GuestAddress kAddrSpaceSize = uint64_t{1} << 48;

enum class PageState : uint8_t {
  Unmapped = 0,
  Reserved,
  Committed,
  ReadOnly,
  ReadWrite,
  Executable,
  GpuVisible,
  Faulting,
  Evicted,
};

enum class Access : uint8_t {
  Read = 1 << 0,
  Write = 1 << 1,
  Execute = 1 << 2,
};
inline Access operator|(Access a, Access b) {
  return Access(uint8_t(a) | uint8_t(b));
}
inline bool HasAccess(uint8_t bits, Access a) {
  return (bits & uint8_t(a)) != 0;
}

struct MemoryTranslation {
  bool ok = false;
  size_t host_offset = 0;  // offset into backing store
  uint64_t version = 0;
};

struct PageInfo {
  PageState state = PageState::Unmapped;
  uint8_t perms = 0;  // Access bits
  uint64_t version = 0;
};

class GuestMemory {
 public:
  static constexpr uint32_t kBdaMaxRetries = 8;

  GuestMemory();

  Result<void> Map(GuestAddress addr, size_t size, PageState state);
  Result<void> Unmap(GuestAddress addr, size_t size);
  Result<void> Protect(GuestAddress addr, size_t size, uint8_t perms);
  Result<void> Synchronize(GuestAddress addr, size_t size);

  Result<MemoryTranslation> Translate(GuestAddress addr, size_t size,
                                      Access access) const;

  Result<void> Read(GuestAddress addr, void* out, size_t size) const;
  Result<void> Write(GuestAddress addr, const void* in, size_t size);
  Result<void> CopyFromGuest(GuestAddress addr, void* out, size_t size) const {
    return Read(addr, out, size);
  }
  Result<void> CopyToGuest(GuestAddress addr, const void* in, size_t size) {
    return Write(addr, in, size);
  }

  // BDA path: page-table -> access -> fault -> bounded retry.
  // resolve_once(idx) should attempt to fix the mapping for attempt idx
  // and return true if progress was made. Never loops forever.
  template <typename F>
  Result<MemoryTranslation> ResolveBda(GuestAddress addr, size_t size, F&& resolve_once);

  [[nodiscard]] uint64_t CpuVersion() const { return cpu_version_; }
  [[nodiscard]] uint64_t GpuVersion() const { return gpu_version_; }
  [[nodiscard]] uint64_t MappingVersion() const { return mapping_version_; }
  [[nodiscard]] PageInfo Query(GuestAddress addr) const;

 private:
  [[nodiscard]] static uint64_t PageBase(GuestAddress a) {
    return a & ~(kPageSize - 1);
  }
  Result<void> CheckRange(GuestAddress addr, size_t size) const;

  std::vector<uint8_t> backing_;          // host backing for mapped range
  GuestAddress base_ = 0;                 // guest base of backing_
  size_t backed_size_ = 0;
  std::map<uint64_t, PageInfo> pages_;    // key: page base
  uint64_t cpu_version_ = 1;
  uint64_t gpu_version_ = 1;
  uint64_t mapping_version_ = 1;
};

template <typename F>
Result<MemoryTranslation> GuestMemory::ResolveBda(GuestAddress addr, size_t size,
                                                 F&& resolve_once) {
  for (uint32_t attempt = 0; attempt < kBdaMaxRetries; ++attempt) {
    auto t = Translate(addr, size, Access::Read);
    if (t.ok()) return t;
    if (t.error != RuntimeError::InvalidGuestMemory) return t;
    bool progress = resolve_once(attempt);
    if (!progress) {
      return Result<MemoryTranslation>::Fail(
          RuntimeError::InvalidGuestMemory,
          "BDA: persistent fault, no progress (attempt " +
              std::to_string(attempt) + ")");
    }
  }
  return Result<MemoryTranslation>::Fail(
      RuntimeError::InvalidGuestMemory,
      "BDA: persistent fault after bounded retries");
}

}  // namespace kyty
