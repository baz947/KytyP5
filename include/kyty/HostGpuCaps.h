#pragma once
// Host GPU capability layer — README §21 (P0).
// Never equate Vulkan 1.3 with one feature set. Each PS5 requirement
// resolves to Native/Lowered/Emulated/Fallback/Unavailable.
#include <cstdint>
#include <string>
#include <unordered_map>
#include "kyty/Compat.h"

namespace kyty::gpu {

struct HostFeatures {
  bool descriptor_arrays = true;
  bool non_uniform_indexing = true;
  bool update_after_bind = false;
  bool partially_bound = true;
  bool bda = true;
  bool ray_query = false;
  bool image_atomics = true;
  bool int64 = true;
  bool float16 = true;
  bool subgroup32 = true;
  bool subgroup64 = false;
};

enum class Requirement : uint8_t {
  DynamicDescriptors = 0,
  NonUniformIndexing,
  Bda,
  RayTracing,
  ImageAtomics,
  Int64,
  Float16,
  Wave64,
};

class HostGpuCaps {
 public:
  explicit HostGpuCaps(HostFeatures f = {}) : host_(f) {}
  GpuCapability Resolve(Requirement r) const;
  [[nodiscard]] uint64_t Hash() const;  // for pipeline-cache keys
  const HostFeatures& features() const { return host_; }

 private:
  HostFeatures host_;
};

}  // namespace kyty::gpu
