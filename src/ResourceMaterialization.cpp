#include "kyty/ResourceMaterialization.h"

namespace kyty::gpu {

MaterializeOutcome ResourceMaterializer::Materialize(
    const CanonicalImage& im, FormatResolution host_res) const {
  if (im.address == 0 || im.format == Format::Unknown)
    return {MaterializeResult::InvalidDescriptor, "null descriptor", 0};
  if (im.resolution == ResolutionState::Invalid)
    return {MaterializeResult::InvalidDescriptor, "invalid resolution", 0};
  if (im.mem_version == 0 && im.resolution == ResolutionState::Runtime)
    return {MaterializeResult::InvalidMemory, "stale BDA version", 0};
  switch (host_res) {
    case FormatResolution::Native:
    case FormatResolution::CompatibleView:
      return {MaterializeResult::Success, "", im.address ^ 0x9e3779b9};
    case FormatResolution::Reinterpreted:
      return {MaterializeResult::NeedViewReinterpretation,
              "reinterpret view", 0};
    case FormatResolution::Emulated:
      return {MaterializeResult::NeedFallback, "fallback:emulated-format", 0};
    case FormatResolution::Unsupported:
      break;
  }
  // Compressed without host support may still run via shader rewrite.
  if (im.format == Format::BC1_UNORM || im.format == Format::BC7_UNORM)
    return {MaterializeResult::NeedShaderRewrite, "decompress-in-shader", 0};
  return {MaterializeResult::Unsupported, "no host path", 0};
}

}  // namespace kyty::gpu
