#include "kyty/RelocationEngine.h"

namespace kyty::linker {

RelocResult RelocationEngine::ApplyOne(
    const Relocation& r, const std::optional<uint64_t>& resolve_target,
    bool dep_loaded, uint64_t load_base, bool allow_stub, uint64_t& out_value,
    Thunk* out_thunk) {
  if (r.offset == 0)
    return RelocResult::Invalid;
  switch (r.type) {
    case RelocType::None:
      return RelocResult::Applied;  // no-op
    case RelocType::Relative: {
      // B + A, no symbol. Always applicable.
      out_value = load_base + uint64_t(r.addend);
      return RelocResult::Applied;
    }
    case RelocType::R64:
    case RelocType::GlobDat:
    case RelocType::JumpSlot: {
      if (resolve_target.has_value()) {
        if (r.type == RelocType::R64)
          out_value = *resolve_target + uint64_t(r.addend);
        else
          out_value = *resolve_target;  // GLOB_DAT/JMP_SLOT: S (+ thunk)
        if (out_thunk && r.type == RelocType::JumpSlot) {
          // PLT slot becomes a thunk carrying ABI/target/policy.
          *out_thunk =
              MakeThunk(SymbolKey{}, 0, *resolve_target, false, "direct");
        }
        return RelocResult::Applied;
      }
      if (!dep_loaded) return RelocResult::Deferred;
      if (allow_stub) {
        out_value = 0;  // semantic stub address (resolved via thunk)
        if (out_thunk) {
          *out_thunk = MakeThunk(SymbolKey{}, 0, 0, true,
                                 "safestub:unresolved-import");
        }
        return RelocResult::Stubbed;
      }
      return RelocResult::Invalid;  // loaded but unresolvable, no stub
    }
    case RelocType::DtpMod64:
    case RelocType::DtpOff64:
    case RelocType::TpOff64: {
      // TLS relocations must never degrade to plain integers.
      // Without a TLS-aware target they defer (not silently Applied).
      if (resolve_target.has_value()) {
        out_value = *resolve_target + uint64_t(r.addend);
        return RelocResult::Applied;
      }
      return RelocResult::Deferred;
    }
    default:
      return RelocResult::Unsupported;
  }
}

}  // namespace kyty::linker
