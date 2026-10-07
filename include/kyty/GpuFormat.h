#pragma once
// GPU formats / views — README §18 (P0).
// Centralized validation; resolver returns Native/CompatibleView/
// Reinterpreted/Emulated/Unsupported (never silent wrong format).
#include <cstdint>
#include <string>
#include "kyty/Compat.h"

namespace kyty::gpu {

enum class Format : uint32_t {
  Unknown = 0,
  R8_UNORM,
  R8G8B8A8_UNORM,
  R8G8B8A8_SRGB,
  B8G8R8A8_UNORM,
  R16_FLOAT,
  R16G16B16A16_FLOAT,
  R32_FLOAT,
  R32G32B32A32_FLOAT,
  D16_UNORM,
  D24_UNORM_S8_UINT,
  D32_FLOAT,
  D32_FLOAT_S8_UINT,
  BC1_UNORM,  // compressed -> Emulated on hosts without support
  BC7_UNORM,
};

enum class Aspect : uint8_t { Color = 0, Depth, Stencil, DepthStencil };
enum class ViewType : uint8_t { Tex1D = 0, Tex2D, Tex3D, Cube, Tex1DArray, Tex2DArray, Tex2DMS, Buffer };

enum class FormatResolution : uint8_t {
  Native = 0,
  CompatibleView,
  Reinterpreted,
  Emulated,
  Unsupported,
};

inline Aspect FormatAspect(Format f) {
  switch (f) {
    case Format::D16_UNORM:
    case Format::D32_FLOAT: return Aspect::Depth;
    case Format::D24_UNORM_S8_UINT:
    case Format::D32_FLOAT_S8_UINT: return Aspect::DepthStencil;
    default: return Aspect::Color;
  }
}

class FormatResolver {
 public:
  // Host capability flags (subset; full set lives in HostGpuCaps).
  struct HostSupport {
    bool srgb = true;
    bool compressed = false;  // BC1/BC7 native?
    bool depth24_s8 = true;
  };
  explicit FormatResolver(HostSupport h = {}) : host_(h) {}
  FormatResolution Resolve(Format guest, Format& out_host) const;
  // View validation: aspect/mip/layer/sample/usage coherence.
  Result<void> ValidateView(Format fmt, ViewType view, uint32_t mips,
                            uint32_t layers, uint32_t samples) const;

 private:
  HostSupport host_;
};

}  // namespace kyty::gpu
