#include "kyty/BackendLowering.h"
#include "kyty/GpuTimeline.h"

namespace kyty::shader {

Result<LoweredShader> BackendLowering::Lower(
    const ShaderInfo& info, const gpu::HostGpuCaps& caps, uint64_t ir_version,
    uint64_t res_model) {
  LoweredShader out;
  out.info = info;
  bool need_wave = info.uses_wave64 &&
                   caps.Resolve(gpu::Requirement::Wave64) !=
                       gpu::GpuCapability::Native;
  bool need_cmp = info.uses_mimg_compare;
  if (need_wave) {
    out.kind = LoweringKind::Wave64Split;
    out.passes = {"wave64.lo", "wave64.hi"};
    out.info.uses_wave64 = false;  // lowered to 2x32
  } else if (need_cmp) {
    out.kind = LoweringKind::MimgCompare;
    out.passes = {"mimg.compare"};
  } else {
    out.kind = LoweringKind::None;
    out.passes = {"native"};
  }
  out.pipeline_key = KeyFor(info.hash, ir_version, res_model, caps.Hash(),
                            need_wave ? "w32x2" : "native",
                            out.kind);
  return Result<LoweredShader>::Ok(std::move(out));
}

std::string BackendLowering::KeyFor(uint64_t shader_hash, uint64_t ir_version,
                                    uint64_t res_model, uint64_t caps_hash,
                                    const std::string& wave_mode,
                                    LoweringKind kind) {
  std::string mode = wave_mode + ":" + std::to_string(int(kind));
  return gpu::GpuTimeline::PipelineKey(shader_hash, ir_version, res_model,
                                       caps_hash, "default-layout", mode);
}

}  // namespace kyty::shader
