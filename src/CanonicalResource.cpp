#include "kyty/CanonicalResource.h"
#include <sstream>

namespace kyty::gpu {

Result<CanonicalImage> CanonicalResource::MakeImage(const CanonicalImage& in) {
  if (in.address == 0)
    return Result<CanonicalImage>::Fail(RuntimeError::InvalidResource,
                                        "null address");
  if (in.format == Format::Unknown)
    return Result<CanonicalImage>::Fail(RuntimeError::InvalidResource,
                                        "unknown format");
  if (in.width == 0 || in.height == 0 || in.depth == 0)
    return Result<CanonicalImage>::Fail(RuntimeError::InvalidResource,
                                        "empty extent");
  if (in.mip_count == 0 || in.base_mip + in.mip_range > in.mip_count)
    return Result<CanonicalImage>::Fail(RuntimeError::InvalidResource,
                                        "bad mip range");
  if (in.array_layers == 0 ||
      in.base_layer + in.layer_range > in.array_layers)
    return Result<CanonicalImage>::Fail(RuntimeError::InvalidResource,
                                        "bad layer range");
  if (in.dimension < 1 || in.dimension > 3)
    return Result<CanonicalImage>::Fail(RuntimeError::InvalidResource,
                                        "bad dimension");
  if (in.resolution == ResolutionState::Invalid)
    return Result<CanonicalImage>::Fail(RuntimeError::InvalidResource,
                                        "invalid resolution");
  return Result<CanonicalImage>::Ok(in);
}

Result<CanonicalImage> CanonicalResource::ReinterpretAs(
    const CanonicalImage& in, ResourceKind kind) {
  CanonicalImage out = in;
  out.kind = kind;
  // BDA alias keeps address/size semantics; Buffer alias drops mip/layers.
  if (kind == ResourceKind::Buffer) {
    out.mip_count = 1;
    out.mip_range = 1;
    out.array_layers = 1;
    out.layer_range = 1;
    out.samples = 1;
  }
  return MakeImage(out);
}

std::string CanonicalResource::CacheKey(const CanonicalImage& im) {
  std::ostringstream o;
  o << std::hex << im.address << ":" << im.mem_version << ":"
    << im.descriptor_fp << ":" << int(im.format) << ":" << im.width << "x"
    << im.height << "x" << im.depth << ":m" << im.mip_count << ":l"
    << im.array_layers << ":u" << im.usage;
  return o.str();
}

}  // namespace kyty::gpu
