#pragma once
// Backend lowering — README §12/§14/§18.
// Expands backend coverage: Wave64 split, MIMG-compare lowering,
// compressed-format shader rewrite, sRGB reinterpret. Each lowering
// is explicit (never silent) and feeds the pipeline-cache key with
// the host-caps hash so lowered variants never collide with native.
#include <cstdint>
#include <string>
#include <vector>
#include "kyty/Compat.h"
#include "kyty/HostGpuCaps.h"
#include "kyty/ShaderCore.h"

namespace kyty::shader {

enum class LoweringKind : uint8_t {
  None = 0,
  Wave64Split,      // 1x64 -> 2x32 passes
  MimgCompare,      // shadow compare -> explicit lod+compare
  CompressedRewrite,// BC decompress in shader
  SrgbReinterpret,  // srgb -> unorm view + convert
};

struct LoweredShader {
  ShaderInfo info;
  std::vector<std::string> passes;  // e.g. {"wave64.lo","wave64.hi"}
  LoweringKind kind = LoweringKind::None;
  std::string pipeline_key;
};

class BackendLowering {
 public:
  // Selects lowering from caps; Native hosts return kind None.
  static Result<LoweredShader> Lower(const ShaderInfo& info,
                                     const gpu::HostGpuCaps& caps,
                                     uint64_t ir_version,
                                     uint64_t res_model);
  static std::string KeyFor(uint64_t shader_hash, uint64_t ir_version,
                            uint64_t res_model, uint64_t caps_hash,
                            const std::string& wave_mode,
                            LoweringKind kind);
};

}  // namespace kyty::shader
