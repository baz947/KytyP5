#pragma once
// Relocation engine — README §8 (P0).
// Independent engine returning Applied/Deferred/Stubbed/Unsupported/Invalid.
// Deferred is essential when a dependency is not yet loaded.
// Call paths: direct address | thunk | lazy resolver | semantic stub.
// A thunk carries symbol/module/ABI/target/failure-policy.
#include <cstdint>
#include <optional>
#include <string>
#include "kyty/Compat.h"
#include "kyty/GuestModule.h"
#include "kyty/SymbolResolver.h"

namespace kyty::linker {

enum class RelocType : uint32_t {
  None = 0,
  R64 = 1,        // S + A
  GlobDat = 6,    // S
  JumpSlot = 7,   // S (PLT)
  Relative = 8,   // B + A (no symbol)
  DtpMod64 = 16,  // TLS module id
  DtpOff64 = 17,  // TLS offset
  TpOff64 = 18,   // TLS tp-relative
};

enum class RelocResult : uint8_t {
  Applied = 0,
  Deferred,    // dependency not loaded yet — retry later
  Stubbed,     // semantic stub installed (records failure policy)
  Unsupported, // known-unknown relocation
  Invalid,     // bad offset/type/addend
};

struct Relocation {
  uint64_t offset = 0;  // guest VA to patch
  RelocType type = RelocType::None;
  uint64_t sym_index = 0;  // 0 = no symbol (e.g. Relative)
  int64_t addend = 0;
};

struct Thunk {
  SymbolKey key;
  ModuleId module = 0;
  std::string abi = "sysv-amd64";
  uint64_t target = 0;  // 0 when stubbed
  bool is_stub = false;
  std::string failure_policy;  // e.g. "deferred", "safestub:libkernel:..."
};

class RelocationEngine {
 public:
  // resolve_target: guest VA of symbol, nullopt when not yet resolvable.
  // dep_loaded: false when the dependency providing the symbol is absent.
  // allow_stub: whether Stubbed is permitted (else Deferred/failed).
  RelocResult ApplyOne(const Relocation& r,
                       const std::optional<uint64_t>& resolve_target,
                       bool dep_loaded, uint64_t load_base, bool allow_stub,
                       uint64_t& out_value, Thunk* out_thunk = nullptr);

  static Thunk MakeThunk(const SymbolKey& key, ModuleId mod, uint64_t target,
                         bool is_stub, std::string policy) {
    return Thunk{key, mod, "sysv-amd64", target, is_stub, std::move(policy)};
  }
};

}  // namespace kyty::linker
