#pragma once
// Resource materialization — README §16 (P0).
// Never boolean fatal/non-fatal. Structured result with recovery options.
// Astro's Playroom CPU-Plaza fatal becomes NeedFallback/rewrite/etc.
#include <string>
#include "kyty/Compat.h"
#include "kyty/CanonicalResource.h"

namespace kyty::gpu {

enum class MaterializeResult : uint8_t {
  Success = 0,
  NeedFallback,
  NeedShaderRewrite,
  NeedViewReinterpretation,
  Unsupported,
  InvalidDescriptor,
  InvalidMemory,
};

struct MaterializeOutcome {
  MaterializeResult result = MaterializeResult::Unsupported;
  std::string action;  // recovery hint, e.g. "fallback:rgba8"
  uint64_t view_id = 0;
};

class ResourceMaterializer {
 public:
  MaterializeOutcome Materialize(const CanonicalImage& im,
                                 FormatResolution host_res) const;
};

}  // namespace kyty::gpu
