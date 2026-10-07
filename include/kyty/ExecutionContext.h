#pragma once
// Guest execution boundary — README §5/§7 (P0).
// All guest->host calls cross a FaultBoundary that converts crashes/
// violations into structured Result + FaultRecord (never raw throw).
#include <cstdint>
#include <functional>
#include "kyty/Compat.h"
#include "kyty/Diagnostics.h"
#include "kyty/FaultManager.h"
#include "kyty/GuestModule.h"

namespace kyty::linker {

struct GuestExecutionContext {
  uint64_t thread_id = 0;
  ModuleId module = 0;
  uint64_t guest_pc = 0;
  Correlation correlation{};
};

class FaultBoundary {
 public:
  template <typename T>
  static Result<T> Invoke(const GuestExecutionContext& ctx,
                          FaultManager& faults, FaultKind kind,
                          std::function<Result<T>()> fn) {
    try {
      Result<T> r = fn();
      if (!r.ok()) {
        FaultRecord rec;
        rec.kind = kind;
        rec.thread = ctx.thread_id;
        rec.guest_pc = ctx.guest_pc;
        rec.correlation = ctx.correlation;
        rec.action = FaultAction::ControlledFailure;
        faults.Record(rec);
      }
      return r;
    } catch (const std::exception&) {
      FaultRecord rec;
      rec.kind = kind;
      rec.thread = ctx.thread_id;
      rec.guest_pc = ctx.guest_pc;
      rec.correlation = ctx.correlation;
      rec.action = FaultAction::ControlledFailure;
      faults.Record(rec);
      return Result<T>::Fail(RuntimeError::InternalInvariant,
                             "fault boundary caught exception");
    } catch (...) {
      return Result<T>::Fail(RuntimeError::InternalInvariant,
                             "fault boundary caught unknown fault");
    }
  }

  static Result<void> InvokeVoid(const GuestExecutionContext& ctx,
                                 FaultManager& faults, FaultKind kind,
                                 std::function<Result<void>()> fn);
};

}  // namespace kyty::linker
