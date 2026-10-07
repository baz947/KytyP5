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

}  // namespace kyty::gpu
