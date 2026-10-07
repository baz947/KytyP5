#include "kyty/DescriptorDecoder.h"

namespace kyty::gpu {

Result<ImageSampleInfo> MimgDecoder::Decode(RawMimg raw) {
  ImageSampleInfo info;
  info.dimension = raw.dim >= 1 && raw.dim <= 3 ? raw.dim : 2;
  info.arrayed = (raw.flags & 0x1) != 0;
  info.multisampled = (raw.flags & 0x2) != 0;
  info.compare = (raw.flags & 0x4) != 0;
  info.has_offset = (raw.flags & 0x10) != 0;
  info.has_clamp = (raw.flags & 0x20) != 0;
  // op class -> lod mode (simplified but explicit + testable).
  switch (raw.op & 0xF) {
    case 0: info.lod_mode = 0; info.derivative_mode = 0; break;
    case 1: info.lod_mode = 1; break;
    case 2: info.lod_mode = 2; break;
    case 3: info.lod_mode = 3; info.derivative_mode = 1; break;
    default:
      return Result<ImageSampleInfo>::Fail(RuntimeError::UnsupportedFeature,
                                           "unknown MIMG class");
  }
  if (info.compare && info.multisampled)
    return Result<ImageSampleInfo>::Fail(RuntimeError::UnsupportedFeature,
                                         "MSAA shadow compare");
  return Result<ImageSampleInfo>::Ok(info);
}

Result<CanonicalImage> DescriptorDecoder::DecodeImage(
    const RawDescriptor& raw, uint64_t mem_version) const {
  // Byte layout (explicit, documented): [0..7] address, [8] format id,
  // [9] dimension, [10..11] w, [12..13] h, [14] mips, [15] layers,
  // [16] samples, [17] usage, [18] swizzle/aspect flags.
  uint64_t addr = 0;
  for (int i = 0; i < 8; ++i) addr |= uint64_t(raw.bytes[i]) << (8 * i);
  if (addr == 0)
    return Result<CanonicalImage>::Fail(RuntimeError::InvalidResource,
                                        "null descriptor address");
  uint8_t fmt_id = raw.bytes[8];
  Format fmt = Format::Unknown;
  switch (fmt_id) {
    case 1: fmt = Format::R8G8B8A8_UNORM; break;
    case 2: fmt = Format::R8G8B8A8_SRGB; break;
    case 3: fmt = Format::R16G16B16A16_FLOAT; break;
    case 4: fmt = Format::D32_FLOAT; break;
    case 5: fmt = Format::R32_FLOAT; break;
    default:
      return Result<CanonicalImage>::Fail(RuntimeError::InvalidResource,
                                          "unknown format id");
  }
  CanonicalImage im;
  im.address = addr;
  im.format = fmt;
  im.dimension = raw.bytes[9] >= 1 && raw.bytes[9] <= 3 ? raw.bytes[9] : 2;
  im.width = uint32_t(raw.bytes[10]) | (uint32_t(raw.bytes[11]) << 8);
  im.height = 1;
  if (im.width == 0) im.width = 1;
  im.mip_count = raw.bytes[14] ? raw.bytes[14] : 1;
  im.mip_range = 1;
  im.array_layers = raw.bytes[15] ? raw.bytes[15] : 1;
  im.layer_range = 1;
  im.samples = raw.bytes[16] ? raw.bytes[16] : 1;
  im.usage = raw.bytes[17];
  im.aspect = FormatAspect(fmt);
  im.mem_version = mem_version;
  // descriptor fingerprint: simple FNV over bytes.
  uint64_t fp = 1469598103934665603ull;
  for (auto b : raw.bytes) {
    fp ^= b;
    fp *= 1099511628211ull;
  }
  im.descriptor_fp = fp;
  im.resolution = ResolutionState::Dynamic;
  return CanonicalResource::MakeImage(im);
}

Result<Uniformity> DescriptorDecoder::ClassifyUniformity(
    uint64_t exec_mask, uint64_t addr_or_index) const {
  (void)addr_or_index;
  if (exec_mask == 0)
    return Result<Uniformity>::Fail(RuntimeError::InvalidArgument,
                                    "empty EXEC");
  // Uniform when a single lane active or value is EXEC-independent
  // (caller passes canonicalized index with high bit set).
  if (addr_or_index >> 63) return Result<Uniformity>::Ok(Uniformity::Uniform);
  // Single-bit EXEC -> trivially uniform.
  if ((exec_mask & (exec_mask - 1)) == 0)
    return Result<Uniformity>::Ok(Uniformity::Uniform);
  return Result<Uniformity>::Ok(Uniformity::Divergent);
}

}  // namespace kyty::gpu
