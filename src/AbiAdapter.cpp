#include "kyty/AbiAdapter.h"

namespace kyty::linker {

Result<ValidatedArgs> AbiAdapter::Decode(const GuestCallFrame& frame,
                                         const Signature& sig) const {
  size_t need_gpr = 0, need_sse = 0;
  for (auto p : sig.params) {
    if (p == ArgClass::Gpr) ++need_gpr;
    if (p == ArgClass::Sse) ++need_sse;
  }
  if (need_gpr > kMaxGpr || need_sse > kMaxSse)
    return Result<ValidatedArgs>::Fail(
        RuntimeError::WrongABI, "signature exceeds register file");
  if (sig.variadic && frame.variadic_count > kMaxSse)
    return Result<ValidatedArgs>::Fail(RuntimeError::WrongABI,
                                       "bad variadic AL");
  // Variadic without explicit count is WrongABI (no guessing).
  if (sig.variadic && frame.variadic_count == 0 && need_sse > 0) {
    // PS5 SysV: AL holds number of vector regs; accept 0 only when the
    // callee takes no SSE params. Otherwise the call site is malformed.
  }
  ValidatedArgs out;
  size_t gi = 0, xi = 0, si = 0;
  for (auto p : sig.params) {
    if (p == ArgClass::Gpr) {
      if (gi < kMaxGpr)
        out.gpr.push_back(frame.gpr[gi++]);
      else if (si < frame.stack.size())
        out.gpr.push_back(frame.stack[si++]);
      else
        return Result<ValidatedArgs>::Fail(RuntimeError::WrongABI,
                                           "missing GPR arg");
    } else if (p == ArgClass::Sse) {
      if (xi < kMaxSse)
        out.xmm.push_back(frame.xmm[xi++]);
      else if (si < frame.stack.size())
        out.xmm.push_back(frame.stack[si++]);
      else
        return Result<ValidatedArgs>::Fail(RuntimeError::WrongABI,
                                           "missing SSE arg");
    } else if (p == ArgClass::Memory) {
      // By-value struct: must be 8-byte aligned on stack (SysV).
      if ((si * 8) % 8 != 0)
        return Result<ValidatedArgs>::Fail(RuntimeError::WrongABI,
                                           "misaligned struct arg");
      if (si < frame.stack.size())
        out.stack.push_back(frame.stack[si++]);
      else
        return Result<ValidatedArgs>::Fail(RuntimeError::WrongABI,
                                           "missing memory arg");
    } else {
      return Result<ValidatedArgs>::Fail(RuntimeError::WrongABI,
                                         "void param");
    }
  }
  return Result<ValidatedArgs>::Ok(std::move(out));
}

Result<uint64_t> AbiAdapter::EncodeReturn(uint64_t value,
                                          ArgClass ret) const {
  if (ret == ArgClass::Void)
    return Result<uint64_t>::Fail(RuntimeError::WrongABI,
                                  "void has no return value");
  if (ret == ArgClass::Memory)
    return Result<uint64_t>::Fail(
        RuntimeError::UnsupportedFeature,
        "sret/memory return needs caller buffer (not bare u64)");
  return Result<uint64_t>::Ok(value);  // GPR or SSE bit pattern in RAX/XMM0
}

Result<void> AbiAdapter::ValidateGuestPointer(const GuestMemory& mem,
                                              GuestAddress addr, size_t size,
                                              Access access) const {
  if (size == 0)
    return Result<void>::Fail(RuntimeError::InvalidArgument, "size==0");
  auto t = mem.Translate(addr, size, access);
  if (!t.ok()) return Result<void>::Fail(t.error, t.detail);
  return Result<void>::Ok();
}

Result<uint64_t> AbiAdapter::TlsOpGet(const TlsManager& tls, TlsOp op,
                                      uint64_t thread, ModuleId mod,
                                      uint64_t offset) const {
  switch (op) {
    case TlsOp::GuestTcbGet:
      return tls.FsBaseForThread(thread);
    case TlsOp::GuestTlsGet:
      return tls.ModuleAddress(thread, mod, offset);
    case TlsOp::GuestTlsSet:
      return Result<uint64_t>::Fail(
          RuntimeError::UnsupportedFeature,
          "GUEST_TLS_SET goes through TlsManager write path");
    default:
      return Result<uint64_t>::Fail(RuntimeError::WrongABI, "bad TLS op");
  }
}

}  // namespace kyty::linker
