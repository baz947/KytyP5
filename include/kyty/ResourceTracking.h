#pragma once
// Resource tracking — README §15 (P0).
// Explicit resolution states; semantic (not structural) expression equality;
// cache keyed on value+CFG+mem-version+wave/path where required.
// Demon's Souls failure class ("not a valid runtime value") becomes a
// structured Invalid rather than a crash.
#include <cstdint>
#include <unordered_map>
#include "kyty/Compat.h"
#include "kyty/CanonicalResource.h"

namespace kyty::gpu {

struct ResourceHandle {
  uint64_t guest_address = 0;
  uint32_t descriptor_index = 0;
  ResolutionState resolution = ResolutionState::Unknown;
  uint32_t cfg_block = 0;
  uint64_t mem_version = 0;
  uint64_t wave_key = 0;  // 0 when wave-invariant
};

class ResourceTracker {
 public:
  Result<ResourceHandle> TrackStatic(uint64_t addr, uint32_t index);
  Result<ResourceHandle> TrackDerived(uint64_t base, uint64_t offset,
                                      uint32_t index, uint32_t cfg_block);
  Result<ResourceHandle> TrackDynamic(uint32_t index, uint32_t cfg_block,
                                      uint64_t mem_version);
  Result<ResourceHandle> TrackRuntime(uint64_t bda_value, uint32_t cfg_block,
                                      uint64_t mem_version, uint64_t wave_key,
                                      bool valid);
  [[nodiscard]] size_t Size() const { return table_.size(); }
  void InvalidateByMemVersion(uint64_t new_version);

 private:
  std::unordered_map<uint64_t, ResourceHandle> table_;  // keyed by id
  uint64_t next_id_ = 1;
};

}  // namespace kyty::gpu
