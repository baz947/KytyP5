#include "kyty/CorpusCompatBridge.h"

namespace kyty::infra {

std::string CorpusCompatBridge::SubsystemFor(const std::string& feature) {
  if (feature == "wave64") return "wave_model";
  if (feature == "dynamic_descriptors") return "resource_tracking";
  if (feature == "image_formats") return "format_resolver";
  if (feature == "depth_stencil") return "format_resolver";
  if (feature == "bda") return "guest_memory";
  if (feature == "atomics") return "atomic_model";
  if (feature == "barriers") return "barrier_model";
  if (feature == "videoout") return "videoout";
  if (feature == "savedata") return "savedata";
  if (feature == "tls") return "tls_manager";
  if (feature == "network") return "network";
  if (feature == "ray_tracing") return "host_caps";
  return "unknown";
}

Result<size_t> CorpusCompatBridge::FileFailures(
    CompatDatabase& db, const std::string& title_id,
    const std::string& version, const std::vector<CorpusResult>& results) {
  auto t = db.Lookup(title_id, version);
  if (!t.ok()) return Result<size_t>::Fail(t.error, t.detail);
  TitleEntry e = t.value;
  size_t n = 0;
  for (auto& r : results) {
    if (r.skipped || r.passed) continue;
    // Find the feature by result name prefix (corpus names are unique).
    std::string subsys = "unknown";
    for (auto& c : BuiltinCorpus()) {
      if (c.name == r.name) {
        subsys = SubsystemFor(c.feature);
        break;
      }
    }
    e.issues.push_back(KnownIssue{subsys, CompatStatus::Broken, r.diff});
    ++n;
  }
  if (auto u = db.Upsert(e); !u.ok())
    return Result<size_t>::Fail(u.error, u.detail);
  return Result<size_t>::Ok(n);
}

CorpusCompatBridge::Evidence CorpusCompatBridge::EvidenceFor(
    CompatDatabase& db, const std::vector<CorpusCase>& corpus,
    const std::string& subsystem) {
  Evidence ev;
  ev.titles = db.AffectedBy(subsystem);
  for (auto& c : corpus) {
    if (SubsystemFor(c.feature) == subsystem) ev.cases.push_back(c);
  }
  return ev;
}

}  // namespace kyty::infra
