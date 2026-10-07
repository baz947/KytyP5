#include "kyty/AtomicBarrierModel.h"

namespace kyty::gpu {

Result<void> MemModel::ValidateAtomic(const AtomicOp& op,
                                      bool host_image_atomics,
                                      bool host_int64) {
  if (op.width != 32 && op.width != 64)
    return Result<void>::Fail(RuntimeError::InvalidArgument, "bad width");
  if (op.width == 64 && !host_int64)
    return Result<void>::Fail(RuntimeError::UnsupportedFeature,
                              "int64 atomics");
  if (op.target == AtomicTarget::Image && !host_image_atomics)
    return Result<void>::Fail(RuntimeError::UnsupportedFeature,
                              "image atomics");
  if (op.is_float && op.width != 32 && op.width != 64)
    return Result<void>::Fail(RuntimeError::InvalidArgument,
                              "bad float width");
  return Result<void>::Ok();
}

Result<void> MemModel::ValidateBarrier(const BarrierOp& b) {
  uint64_t lanes = b.wave_size == 64 ? 64 : 32;
  uint64_t full = lanes == 64 ? ~0ull : ((1ull << 32) - 1);
  uint64_t active = b.exec_mask & full;
  if (active == 0)
    return Result<void>::Fail(RuntimeError::InvalidArgument, "empty EXEC");
  if (active != full)
    return Result<void>::Fail(
        RuntimeError::ShaderFailure,
        "divergent barrier: not all lanes reach it");
  return Result<void>::Ok();
}

}  // namespace kyty::gpu
