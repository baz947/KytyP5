#include "kyty/ExecutionContext.h"

namespace kyty::linker {

Result<void> FaultBoundary::InvokeVoid(
    const GuestExecutionContext& ctx, FaultManager& faults, FaultKind kind,
    std::function<Result<void>()> fn) {
  return Invoke<void>(
      ctx, faults, kind,
      [&]() -> Result<void> {
        Result<void> r = fn();
        if (!r.ok()) return r;
        return Result<void>::Ok();
      });
}

}  // namespace kyty::linker
