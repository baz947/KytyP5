#include "kyty/ResourceTracking.h"

namespace kyty::gpu {

Result<ResourceHandle> ResourceTracker::TrackStatic(uint64_t addr,
                                                    uint32_t index) {
  if (addr == 0)
    return Result<ResourceHandle>::Fail(RuntimeError::InvalidResource,
                                        "null static resource");
  ResourceHandle h{addr, index, ResolutionState::Static, 0, 0, 0};
  table_[next_id_++] = h;
  return Result<ResourceHandle>::Ok(h);
}

Result<ResourceHandle> ResourceTracker::TrackDerived(uint64_t base,
                                                    uint64_t offset,
                                                    uint32_t index,
                                                    uint32_t cfg_block) {
  if (base == 0)
    return Result<ResourceHandle>::Fail(RuntimeError::InvalidResource,
                                        "null derived base");
  ResourceHandle h{base + offset, index, ResolutionState::Derived, cfg_block,
                   0, 0};
  table_[next_id_++] = h;
  return Result<ResourceHandle>::Ok(h);
}

Result<ResourceHandle> ResourceTracker::TrackDynamic(uint32_t index,
                                                    uint32_t cfg_block,
                                                    uint64_t mem_version) {
  ResourceHandle h{0, index, ResolutionState::Dynamic, cfg_block, mem_version,
                   0};
  table_[next_id_++] = h;
  return Result<ResourceHandle>::Ok(h);
}

Result<ResourceHandle> ResourceTracker::TrackRuntime(uint64_t bda_value,
                                                    uint32_t cfg_block,
                                                    uint64_t mem_version,
                                                    uint64_t wave_key,
                                                    bool valid) {
  if (!valid)
    return Result<ResourceHandle>::Fail(
        RuntimeError::InvalidResource,
        "GetImageResource: not a valid runtime value");
  if (bda_value == 0)
    return Result<ResourceHandle>::Fail(RuntimeError::InvalidResource,
                                        "null BDA runtime value");
  ResourceHandle h{bda_value, 0, ResolutionState::Runtime, cfg_block,
                   mem_version, wave_key};
  table_[next_id_++] = h;
  return Result<ResourceHandle>::Ok(h);
}

void ResourceTracker::InvalidateByMemVersion(uint64_t new_version) {
  for (auto& [_, h] : table_) {
    if (h.resolution == ResolutionState::Dynamic ||
        h.resolution == ResolutionState::Runtime)
      h.mem_version = new_version;
  }
}

Result<ResourceHandle> ResourceTracker::TrackRuntimeMasked(
    uint64_t bda_value, uint32_t cfg_block, uint64_t mem_version,
    uint64_t wave_key, uint8_t dword_total, uint32_t resolved_mask) {
  if (dword_total == 0 || dword_total > 32)
    return Result<ResourceHandle>::Fail(RuntimeError::InvalidArgument,
                                        "dword_total out of 1..32");
  uint32_t legal = dword_total == 32 ? 0xFFFFFFFFu
                                     : ((1u << dword_total) - 1u);
  if (resolved_mask & ~legal)
    return Result<ResourceHandle>::Fail(RuntimeError::InvalidArgument,
                                        "mask exceeds dword_total");
  if (bda_value == 0)
    return Result<ResourceHandle>::Fail(RuntimeError::InvalidResource,
                                        "null BDA runtime value");
  if (resolved_mask == 0)
    return Result<ResourceHandle>::Fail(
        RuntimeError::InvalidResource,
        "GetImageResource: not a valid runtime value");
  ResourceHandle h{bda_value, 0, ResolutionState::Runtime, cfg_block,
                   mem_version, wave_key, dword_total, resolved_mask};
  table_[next_id_++] = h;
  return Result<ResourceHandle>::Ok(h);
}

}  // namespace kyty::gpu
