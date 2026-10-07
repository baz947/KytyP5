#pragma once
// Cross-subsystem trace correlation — README §26
#include <cstdint>

namespace kyty {

struct Correlation {
  uint64_t frame = 0;
  uint64_t thread = 0;
  uint64_t runtime_call = 0;
  uint64_t gpu_submit = 0;
  uint64_t shader_hash = 0;
  uint64_t resource_id = 0;
  uint64_t guest_address = 0;
  uint64_t gpu_timeline = 0;
};

}  // namespace kyty
