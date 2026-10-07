#pragma once
// Descriptor decoder + MIMG semantics — README §13/§18 (P0).
// Semantic metadata (not one impl per raw encoding), independently testable.
// Dynamic path: shader -> guest addr -> decode -> canonical -> caps -> view.
// Uniformity: Uniform/Divergent/Unknown (non-uniform indexing depends on it).
#include <array>
#include <cstdint>
#include "kyty/Compat.h"
#include "kyty/CanonicalResource.h"

namespace kyty::gpu {

enum class Uniformity : uint8_t { Uniform = 0, Divergent, Unknown };

// MIMG semantic metadata (covers SAMPLE/GATHER/ATOMIC image ops).
struct ImageSampleInfo {
  uint8_t dimension = 2;  // 1/2/3/cube-encoded-as-2array
  bool arrayed = false;
  bool multisampled = false;
  bool compare = false;       // depth-compare (shadow)
  uint8_t lod_mode = 0;       // 0 fixed, 1 bias, 2 lod, 3 gradients
  uint8_t derivative_mode = 0;// 0 implicit, 1 explicit, 2 coarse
  bool has_offset = false;
  bool has_clamp = false;
};

// Minimal raw MIMG word pair (opcode + addr/flags), decoded to semantics.
struct RawMimg {
  uint32_t op = 0;      // image opcode class
  uint32_t flags = 0;   // bit0 arrayed, bit1 msaa, bit2 compare, bit4 offset...
  uint8_t dim = 2;
};

class MimgDecoder {
 public:
  static Result<ImageSampleInfo> Decode(RawMimg raw);
};

// 64-byte PS5-style image descriptor (simplified but explicit).
struct RawDescriptor {
  std::array<uint8_t, 64> bytes = {};
};

class DescriptorDecoder {
 public:
  Result<CanonicalImage> DecodeImage(const RawDescriptor& raw,
                                     uint64_t mem_version) const;
  Result<Uniformity> ClassifyUniformity(uint64_t exec_mask,
                                        uint64_t addr_or_index) const;
};

}  // namespace kyty::gpu
