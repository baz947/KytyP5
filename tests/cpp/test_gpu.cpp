#include <cassert>
#include <cstdio>
#include "kyty/AtomicBarrierModel.h"
#include "kyty/CanonicalResource.h"
#include "kyty/DescriptorDecoder.h"
#include "kyty/GpuFormat.h"
#include "kyty/GpuTimeline.h"
#include "kyty/HostGpuCaps.h"
#include "kyty/ResourceMaterialization.h"
#include "kyty/ResourceTracking.h"
#include "kyty/ShaderHangDetector.h"
#include "kyty/WaveModel.h"

int main() {
  using namespace kyty;
  using namespace kyty::gpu;

  // Formats: native vs emulated vs unsupported
  FormatResolver fr(FormatResolver::HostSupport{true, false, true});
  Format host = Format::Unknown;
  assert(fr.Resolve(Format::R8G8B8A8_UNORM, host) == FormatResolution::Native);
  assert(fr.Resolve(Format::BC1_UNORM, host) == FormatResolution::Emulated);
  assert(fr.Resolve(Format::Unknown, host) == FormatResolution::Unsupported);
  assert(!fr.ValidateView(Format::Unknown, ViewType::Tex2D, 1, 1, 1).ok());
  assert(!fr.ValidateView(Format::R8G8B8A8_UNORM, ViewType::Tex2D, 0, 1, 1).ok());

  // Canonical image + aliasing + cache key versioning
  CanonicalImage im;
  im.address = 0x10000;
  im.format = Format::R8G8B8A8_UNORM;
  im.width = 256;
  im.mip_count = 4;
  im.array_layers = 1;
  im.mem_version = 7;
  im.descriptor_fp = 123;
  assert(CanonicalResource::MakeImage(im).ok());
  CanonicalImage bad = im;
  bad.width = 0;
  assert(!CanonicalResource::MakeImage(bad).ok());
  auto as_buf = CanonicalResource::ReinterpretAs(im, ResourceKind::Buffer);
  assert(as_buf.ok() && as_buf.value.mip_count == 1);
  assert(CanonicalResource::CacheKey(im) !=
         CanonicalResource::CacheKey(
             [&] { auto c = im; c.mem_version = 8; return c; }()));

  // MIMG decode + unsupported class
  assert(MimgDecoder::Decode(RawMimg{0, 0x1, 2}).ok().value.arrayed);
  assert(!MimgDecoder::Decode(RawMimg{0xF, 0, 2}).ok());
  assert(!MimgDecoder::Decode(RawMimg{0, 0x2 | 0x4, 2}).ok());  // msaa shadow

  // Descriptor decode + uniformity
  RawDescriptor raw;
  for (int i = 0; i < 8; ++i) raw.bytes[i] = uint8_t(0x10000 >> (8 * i));
  raw.bytes[8] = 1;  // RGBA8
  raw.bytes[9] = 2;
  raw.bytes[10] = 0x00;
  raw.bytes[11] = 0x01;  // 256
  raw.bytes[14] = 4;
  DescriptorDecoder dd;
  auto di = dd.DecodeImage(raw, 7);
  assert(di.ok() && di.value.width == 256);
  RawDescriptor nullraw;
  assert(!dd.DecodeImage(nullraw, 1).ok());
  assert(dd.ClassifyUniformity(0x1, 0).ok().value == Uniformity::Uniform);
  assert(dd.ClassifyUniformity(0x3, 0).ok().value == Uniformity::Divergent);

  // Tracking: Demon's Souls class -> structured Invalid
  ResourceTracker tr;
  assert(tr.TrackStatic(0x1000, 0).ok());
  assert(!tr.TrackStatic(0, 0).ok());
  assert(!tr.TrackRuntime(0, 1, 1, 0, false).ok());
  auto rt = tr.TrackRuntime(0xBDA0, 3, 9, 0xABC, true);
  assert(rt.ok() && rt.value.resolution == ResolutionState::Runtime);

  // Materialization: structured, never boolean
  ResourceMaterializer mz;
  auto ok = mz.Materialize(im, FormatResolution::Native);
  assert(ok.result == MaterializeResult::Success);
  CanonicalImage bc = im;
  bc.format = Format::BC1_UNORM;
  assert(mz.Materialize(bc, FormatResolution::Emulated).result ==
         MaterializeResult::NeedFallback);
  CanonicalImage nullim;
  assert(mz.Materialize(nullim, FormatResolution::Native).result ==
         MaterializeResult::InvalidDescriptor);

  // Wave: no silent degradation
  assert(WaveModel::Make(WaveSize::Wave32, 0x1).ok());
  assert(!WaveModel::Make(WaveSize::Wave32, 0).ok());
  assert(WaveModel::SelectBackend(WaveSize::Wave64, false, true).ok().value ==
         WaveBackend::Split);
  assert(!WaveModel::SelectBackend(WaveSize::Wave64, false, false).ok());
  auto w = WaveModel::Make(WaveSize::Wave32, 0x5).value;
  assert(WaveModel::IsLaneActive(w, 0) && !WaveModel::IsLaneActive(w, 1));

  // Atomics/barriers: divergent barrier is ShaderFailure
  AtomicOp aop{1, 32, false, AtomicTarget::Image, MemScope::Device,
               MemOrder::Relaxed};
  assert(MemModel::ValidateAtomic(aop, true, true).ok());
  assert(!MemModel::ValidateAtomic(aop, false, true).ok());
  BarrierOp full{MemScope::Workgroup, MemScope::Device,
                 MemOrder::AcquireRelease, 0, 0xFFFFFFFFu, 32};
  assert(MemModel::ValidateBarrier(full).ok());
  BarrierOp div = full;
  div.exec_mask = 0x1;
  assert(!MemModel::ValidateBarrier(div).ok());

  // Host caps: per-requirement resolution
  HostGpuCaps caps(HostFeatures{});
  assert(caps.Resolve(Requirement::DynamicDescriptors) ==
         GpuCapability::Native);
  HostFeatures no64 = HostFeatures{};
  no64.subgroup64 = false;
  assert(HostGpuCaps(no64).Resolve(Requirement::Wave64) ==
         GpuCapability::Lowered);
  HostFeatures noq = HostFeatures{};
  noq.ray_query = false;
  assert(HostGpuCaps(noq).Resolve(Requirement::RayTracing) ==
         GpuCapability::Unavailable);
  assert(caps.Hash() != HostGpuCaps(HostFeatures{false}).Hash());

  // Timeline: lifetime obeys completion
  GpuTimeline tl;
  auto h = tl.Submit(10, 0, 0xABCD, "resA");
  assert(h.ok());
  assert(!tl.CanDestroy("resA"));
  assert(tl.GuestToHost(10) == h.value);
  assert(tl.Complete(h.value).ok() && tl.IsComplete(h.value));
  assert(tl.CanDestroy("resA"));
  assert(!GpuTimeline::BufferKey(1, 2, 3, 4, "x").empty());
  assert(!GpuTimeline::PipelineKey(1, 2, 3, 4, "l", "w32").empty());

  // Hang detector: below cap rejected, above returns evidence
  ShaderHangDetector hd;
  assert(!hd.Observe(1, "ps", 0x100, 32, 0xF, 0x80, 10).ok());
  assert(hd.Observe(1, "ps", 0x100, 32, 0xF, 0x80,
                    ShaderHangDetector::kLoopCap + 1)
             .ok());

  std::puts("test_gpu OK");
  return 0;
}
