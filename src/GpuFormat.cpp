#include "kyty/GpuFormat.h"

namespace kyty::gpu {

FormatResolution FormatResolver::Resolve(Format guest,
                                         Format& out_host) const {
  switch (guest) {
    case Format::Unknown:
      return FormatResolution::Unsupported;
    case Format::R8_UNORM:
    case Format::R8G8B8A8_UNORM:
    case Format::B8G8R8A8_UNORM:
    case Format::R16_FLOAT:
    case Format::R16G16B16A16_FLOAT:
    case Format::R32_FLOAT:
    case Format::R32G32B32A32_FLOAT:
    case Format::D16_UNORM:
    case Format::D32_FLOAT:
      out_host = guest;
      return FormatResolution::Native;
    case Format::R8G8B8A8_SRGB:
      out_host = host_.srgb ? guest : Format::R8G8B8A8_UNORM;
      return host_.srgb ? FormatResolution::Native
                        : FormatResolution::Reinterpreted;
    case Format::D24_UNORM_S8_UINT:
      out_host = host_.depth24_s8 ? guest : Format::D32_FLOAT_S8_UINT;
      return host_.depth24_s8 ? FormatResolution::Native
                              : FormatResolution::CompatibleView;
    case Format::D32_FLOAT_S8_UINT:
      out_host = guest;
      return FormatResolution::Native;
    case Format::BC1_UNORM:
    case Format::BC7_UNORM:
      out_host = guest;
      return host_.compressed ? FormatResolution::Native
                              : FormatResolution::Emulated;
    default:
      return FormatResolution::Unsupported;
  }
}

Result<void> FormatResolver::ValidateView(Format fmt, ViewType view,
                                          uint32_t mips, uint32_t layers,
                                          uint32_t samples) const {
  if (fmt == Format::Unknown)
    return Result<void>::Fail(RuntimeError::InvalidResource, "bad format");
  if (mips == 0 || mips > 16 || layers == 0 || layers > 2048)
    return Result<void>::Fail(RuntimeError::InvalidResource,
                              "bad mip/layer range");
  if (samples != 1 && samples != 2 && samples != 4 && samples != 8)
    return Result<void>::Fail(RuntimeError::InvalidResource, "bad samples");
  bool depth = FormatAspect(fmt) != Aspect::Color;
  if (depth && samples > 1 && fmt == Format::D32_FLOAT_S8_UINT)
    return Result<void>::Fail(RuntimeError::UnsupportedFeature,
                              "MSAA depth-stencil view");
  (void)view;
  return Result<void>::Ok();
}

}  // namespace kyty::gpu
