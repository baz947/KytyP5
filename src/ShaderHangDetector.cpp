#include "kyty/ShaderHangDetector.h"

namespace kyty::gpu {

Result<LoopEvidence> ShaderHangDetector::Observe(
    uint64_t shader_hash, std::string stage, uint64_t pc, uint64_t wave_size,
    uint64_t exec, uint64_t loop_header, uint64_t iterations) {
  if (shader_hash == 0)
    return Result<LoopEvidence>::Fail(RuntimeError::InvalidArgument,
                                      "no shader");
  if (iterations < kLoopCap)
    return Result<LoopEvidence>::Fail(RuntimeError::InvalidArgument,
                                      "below loop cap (no hang)");
  LoopEvidence e;
  e.shader_hash = shader_hash;
  e.stage = std::move(stage);
  e.pc = pc;
  e.wave_size = wave_size;
  e.exec = exec;
  e.loop_header = loop_header;
  e.iterations = iterations;
  return Result<LoopEvidence>::Ok(std::move(e));
}

}  // namespace kyty::gpu
