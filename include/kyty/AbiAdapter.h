#pragma once
// ABI adapter (SysV AMD64) — README §7 (P0).
// Flow: Guest Call -> ABI Decode -> Validated Args -> Impl -> ABI Encode.
// Guest pointers always validated through GuestMemory, never host casts.
// Variadic calls require explicit count; TLS access via TlsManager opcodes.
#include <array>
#include <cstdint>
#include <vector>
#include "kyty/Compat.h"
#include "kyty/GuestMemory.h"
#include "kyty/TlsManager.h"

namespace kyty::linker {

enum class ArgClass : uint8_t { Gpr = 0, Sse, Memory, Void };

struct Signature {
  std::vector<ArgClass> params;
  ArgClass ret = ArgClass::Gpr;
  bool variadic = false;
};

// Raw guest register state for one call site.
struct GuestCallFrame {
  std::array<uint64_t, 6> gpr = {};  // RDI RSI RDX RCX R8 R9
  std::array<uint64_t, 8> xmm = {};  // XMM0..7 bit patterns
  std::vector<uint64_t> stack;       // stack qwords (past return addr)
  uint32_t variadic_count = 0;       // AL for variadic calls
};

struct ValidatedArgs {
  std::vector<uint64_t> gpr;
  std::vector<uint64_t> xmm;
  std::vector<uint64_t> stack;
};

enum class TlsOp : uint8_t {
  GuestTlsGet = 0,  // GUEST_TLS_GET
  GuestTlsSet = 1,  // GUEST_TLS_SET
  GuestTcbGet = 2,  // GUEST_TCB_GET
};

class AbiAdapter {
 public:
  static constexpr size_t kMaxGpr = 6;
  static constexpr size_t kMaxSse = 8;

  Result<ValidatedArgs> Decode(const GuestCallFrame& frame,
                               const Signature& sig) const;
  Result<uint64_t> EncodeReturn(uint64_t value, ArgClass ret) const;

  // Guest pointer validation (read/write/exec) through GuestMemory.
  Result<void> ValidateGuestPointer(const GuestMemory& mem, GuestAddress addr,
                                    size_t size, Access access) const;

  // TLS opcodes behind the adapter (fs: rewriting stays here).
  Result<uint64_t> TlsOpGet(const TlsManager& tls, TlsOp op, uint64_t thread,
                            ModuleId mod, uint64_t offset) const;
};

}  // namespace kyty::linker
