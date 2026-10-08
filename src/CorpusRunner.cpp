#include "kyty/CorpusRunner.h"
#include <filesystem>
#include <fstream>

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

std::vector<CorpusCase> LoadManifests(
    const std::vector<std::string>& candidate_roots) {
  namespace fs = std::filesystem;
  for (auto& root : candidate_roots) {
    std::error_code ec;
    if (!fs::is_directory(root, ec)) continue;
    std::vector<CorpusCase> out;
    for (auto& de : fs::directory_iterator(root, ec)) {
      if (ec || !de.is_directory(ec)) continue;
      std::ifstream f(de.path() / "manifest");
      if (!f) continue;
      std::string feature = de.path().filename().string();
      std::string line;
      while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        std::vector<std::string> parts;
        size_t pos = 0;
        while (true) {
          size_t bar = line.find('|', pos);
          parts.push_back(line.substr(pos, bar == std::string::npos
                                                 ? bar
                                                 : bar - pos));
          if (bar == std::string::npos) break;
          pos = bar + 1;
        }
        if (parts.size() != 4) continue;  // malformed -> skip, never crash
        out.push_back(
            CorpusCase{feature, parts[0], parts[1], parts[2], parts[3]});
      }
    }
    if (!out.empty()) return out;
  }
  return {};
}

}  // namespace kyty::infra
