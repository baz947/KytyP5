#include "kyty/CorpusRunner.h"

namespace kyty::infra {

bool CorpusRunner::RequirementMet(const std::string& req) const {
  using gpu::GpuCapability;
  auto need = [&](gpu::Requirement r) {
    return caps_.Resolve(r) != GpuCapability::Unavailable;
  };
  if (req == "Wave64") return need(gpu::Requirement::Wave64);
  if (req == "Bda") return need(gpu::Requirement::Bda);
  if (req == "DynamicDescriptors")
    return need(gpu::Requirement::DynamicDescriptors);
  if (req == "RayTracing") return need(gpu::Requirement::RayTracing);
  if (req == "ImageAtomics") return need(gpu::Requirement::ImageAtomics);
  if (req == "None" || req.empty()) return true;
  return false;
}

std::vector<CorpusResult> CorpusRunner::Run(const Harness& h) const {
  std::vector<CorpusResult> out;
  for (auto& c : cases_) {
    CorpusResult r;
    r.name = c.name;
    if (!RequirementMet(c.requirement)) {
      r.skipped = true;
      continue;
    }
    Result<std::string> a = h(c);
    if (!a.ok()) {
      r.actual = std::string("ERROR:") + ToString(a.error) + ":" + a.detail;
      r.diff = "harness-error vs " + c.expected;
      continue;
    }
    r.actual = a.value;
    r.passed = (r.actual == c.expected);
    if (!r.passed) r.diff = "actual '" + r.actual + "' != '" + c.expected + "'";
  }
  return out;
}

std::vector<CorpusCase> BuiltinCorpus() {
  return {
      {"wave64", "wave64.split_basic", "Wave64", "wave64:split",
       "backend=split"},
      {"dynamic_descriptors", "desc.bindless_0", "DynamicDescriptors",
       "desc:idx=0", "view=ok"},
      {"image_formats", "fmt.bc1_emu", "None", "fmt=BC1", "fallback=emu"},
      {"depth_stencil", "ds.d32", "None", "fmt=D32", "native"},
      {"bda", "bda.resolve_ok", "Bda", "bda:valid", "addr=ok"},
      {"atomics", "atom.img_add", "ImageAtomics", "atomic:image", "ok"},
      {"barriers", "barrier.full", "None", "exec=full", "ok"},
      {"videoout", "vo.flip_paced", "None", "flip:60fps", "vblank+1"},
      {"savedata", "save.commit", "None", "stage+commit", "read=ok"},
      {"tls", "tls.isolated", "None", "2threads", "addr!=addr"},
      {"network", "net.loopback", "None", "send/recv", "echo=ok"},
      {"ray_tracing", "rt.query", "RayTracing", "ray:query", "lowered"},
  };
}

}  // namespace kyty::infra
