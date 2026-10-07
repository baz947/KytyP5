#pragma once
// Atomics / barriers — README §19 (P0).
// Opcode/width/type/scope/semantics/target explicit; divergent barriers
// detected (EXEC-dependent barrier is a semantic failure, not skipped).
#include <cstdint>
#include "kyty/Compat.h"

namespace kyty::gpu {

enum class AtomicTarget : uint8_t { Buffer = 0, Image, Lds, Global, Bda };
enum class MemScope : uint8_t {
  Invocation = 0, Subgroup, Workgroup, Device, System
};
enum class MemOrder : uint8_t {
  Relaxed = 0, Acquire, Release, AcquireRelease, SeqCst
};

struct AtomicOp {
  uint32_t opcode = 0;  // add/and/or/xor/min/max/cas/...
  uint8_t width = 32;   // 32/64
  bool is_float = false;
  AtomicTarget target = AtomicTarget::Buffer;
  MemScope scope = MemScope::Device;
  MemOrder order = MemOrder::Relaxed;
};

struct BarrierOp {
  MemScope exec_scope = MemScope::Workgroup;
  MemScope mem_scope = MemScope::Device;
  MemOrder order = MemOrder::AcquireRelease;
  uint64_t visibility_mask = 0;  // resources made visible
  uint64_t exec_mask = 0;        // EXEC at barrier site
  uint64_t wave_size = 32;
};

class MemModel {
 public:
  static Result<void> ValidateAtomic(const AtomicOp& op,
                                     bool host_image_atomics,
                                     bool host_int64);
  // Divergent barrier: not all lanes in wave reach it.
  static Result<void> ValidateBarrier(const BarrierOp& b);
};

}  // namespace kyty::gpu
