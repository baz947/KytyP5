#include "kyty/HostGpuCaps.h"

namespace kyty::gpu {

GpuCapability HostGpuCaps::Resolve(Requirement r) const {
  switch (r) {
    case Requirement::DynamicDescriptors:
      if (host_.descriptor_arrays && host_.partially_bound)
        return GpuCapability::Native;
      if (host_.descriptor_arrays) return GpuCapability::Lowered;
      return GpuCapability::Emulated;
    case Requirement::NonUniformIndexing:
      return host_.non_uniform_indexing ? GpuCapability::Native
                                        : GpuCapability::Emulated;
    case Requirement::Bda:
      return host_.bda ? GpuCapability::Native : GpuCapability::Emulated;
    case Requirement::RayTracing:
      return host_.ray_query ? GpuCapability::Lowered
                             : GpuCapability::Unavailable;
    case Requirement::ImageAtomics:
      return host_.image_atomics ? GpuCapability::Native
                                 : GpuCapability::Fallback;
    case Requirement::Int64:
      return host_.int64 ? GpuCapability::Native : GpuCapability::Emulated;
    case Requirement::Float16:
      return host_.float16 ? GpuCapability::Native : GpuCapability::Emulated;
    case Requirement::Wave64:
      if (host_.subgroup64) return GpuCapability::Native;
      if (host_.subgroup32) return GpuCapability::Lowered;
      return GpuCapability::Emulated;
    default:
      return GpuCapability::Unavailable;
  }
}

uint64_t HostGpuCaps::Hash() const {
  uint64_t h = 1469598103934665603ull;
  auto mix = [&](bool b) {
    h ^= b ? 0x9e3779b9 : 0x85ebca6b;
    h *= 1099511628211ull;
  };
  mix(host_.descriptor_arrays);
  mix(host_.non_uniform_indexing);
  mix(host_.update_after_bind);
  mix(host_.partially_bound);
  mix(host_.bda);
  mix(host_.ray_query);
  mix(host_.image_atomics);
  mix(host_.int64);
  mix(host_.float16);
  mix(host_.subgroup32);
  mix(host_.subgroup64);
  return h;
}

}  // namespace kyty::gpu
