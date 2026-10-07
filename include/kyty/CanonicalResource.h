#pragma once
// Canonical resource / image model — README §17 (P0).
// One guest allocation may alias Buffer/Image/Storage/BDA: explicit.
// Cache keys include addr/range/mem-version/descriptor-fp/format/dims.
#include <cstdint>
#include <string>
#include "kyty/Compat.h"
#include "kyty/GpuFormat.h"

namespace kyty::gpu {

enum class ResourceKind : uint8_t { Buffer = 0, Image, Storage, Bda };
enum class ResolutionState : uint8_t {
  Static = 0,   // fully known at compile time
  Derived,      // computed from constants + block id
  Dynamic,      // descriptor-indexed, decoded at bind time
  Runtime,      // BDA / wave-dependent, resolved at submit
  Unknown,
  Invalid,
};

struct CanonicalImage {
  uint64_t address = 0;
  Format format = Format::Unknown;
  uint8_t dimension = 2;  // 1/2/3
  uint32_t width = 0, height = 1, depth = 1;
  uint32_t mip_count = 1;
  uint32_t array_layers = 1;
  uint32_t base_mip = 0, mip_range = 1;
  uint32_t base_layer = 0, layer_range = 1;
  uint32_t samples = 1;
  Aspect aspect = Aspect::Color;
  uint32_t swizzle = 0;  // packed 4x3bit
  uint32_t usage = 0;    // bit: sampled|storage|rt|ds|...
  bool fmask = false, htile = false;
  bool is_depth = false, stencil = false;
  ResolutionState resolution = ResolutionState::Unknown;
  ResourceKind kind = ResourceKind::Image;
  uint64_t mem_version = 0;
  uint64_t descriptor_fp = 0;
};

class CanonicalResource {
 public:
  static Result<CanonicalImage> MakeImage(const CanonicalImage& in);
  // Aliasing: same allocation viewed as Buffer vs Image vs BDA.
  static Result<CanonicalImage> ReinterpretAs(const CanonicalImage& in,
                                              ResourceKind kind);
  static std::string CacheKey(const CanonicalImage& im);
};

}  // namespace kyty::gpu
