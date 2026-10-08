#pragma once
// Real SPIR-V backend (minimal, valid) + structural validator.
// Replaces the fingerprint stub: straight-line uint32 ALU IR emits real
// SPIR-V 1.3 words (single source of truth for opcodes below).
// Honest limits: the demo path is untyped-uint32; ops needing capabilities
// (ImageSample/Atomic/Barrier/lane ops) return structured UnsupportedFeature
// instead of silent wrong emission. The validator checks structure
// (magic/version/bound/lengths/pairing/id bounds), not full spirv-val.
#include <cstdint>
#include <string>
#include <vector>
#include "kyty/Compat.h"
#include "kyty/ShaderCore.h"

namespace kyty::shader::spirv {

// SPIR-V 1.3 opcode numbers (single table shared by emitter + validator).
inline constexpr uint32_t kMagic = 0x07230203;
inline constexpr uint32_t kVersion13 = 0x00010300;
inline constexpr uint32_t kOpCapability = 17;
inline constexpr uint32_t kOpMemoryModel = 14;
inline constexpr uint32_t kOpEntryPoint = 15;
inline constexpr uint32_t kOpExecutionMode = 16;
inline constexpr uint32_t kOpTypeVoid = 19;
inline constexpr uint32_t kOpTypeInt = 21;
inline constexpr uint32_t kOpTypeFunction = 33;
inline constexpr uint32_t kOpConstant = 43;
inline constexpr uint32_t kOpFunction = 54;
inline constexpr uint32_t kOpFunctionParameter = 55;
inline constexpr uint32_t kOpFunctionEnd = 56;
inline constexpr uint32_t kOpCopyObject = 83;
inline constexpr uint32_t kOpIAdd = 124;
inline constexpr uint32_t kOpIMul = 127;
inline constexpr uint32_t kOpLabel = 248;
inline constexpr uint32_t kOpReturn = 253;

struct SpirvModule {
  std::vector<uint32_t> words;
  uint32_t bound = 0;
};

class SpirvEmitter {
 public:
  static Result<SpirvModule> Emit(const IrModule& ir,
                                  const std::string& stage);
};

class SpirvValidator {
 public:
  static Result<void> Validate(const uint32_t* words, size_t count);
  static Result<void> Validate(const SpirvModule& m) {
    return Validate(m.words.data(), m.words.size());
  }
};

}  // namespace kyty::shader::spirv
