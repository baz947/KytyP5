#pragma once
// Persistent shader-loop diagnostics — README §20 (P0).
// Turns an apparent GPU hang into a reproducible semantic failure.
#include <cstdint>
#include <string>
#include <vector>
#include "kyty/Compat.h"

namespace kyty::gpu {

struct LoopEvidence {
  uint64_t shader_hash = 0;
  std::string stage;
  uint64_t pc = 0;
  uint64_t wave_size = 32;
  uint64_t exec = 0;
  uint64_t loop_header = 0;
  uint64_t iterations = 0;
  uint64_t atomics = 0;
  std::vector<uint64_t> resources;
  uint64_t changed_fp = 0, unchanged_fp = 0;
};

class ShaderHangDetector {
 public:
  static constexpr uint64_t kLoopCap = 1'000'000;
  Result<LoopEvidence> Observe(uint64_t shader_hash, std::string stage,
                               uint64_t pc, uint64_t wave_size, uint64_t exec,
                               uint64_t loop_header, uint64_t iterations);
};

}  // namespace kyty::gpu
