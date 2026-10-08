#include "kyty/SoakRunner.h"
#include "kyty/CorpusCompatBridge.h"

namespace kyty::infra {

Result<void> SoakRunner::ApplyChaos(const std::string& feature) const {
  if (feature == "bda") return chaos_.BeforeMemAccess();
  if (feature == "image_formats" || feature == "depth_stencil" ||
      feature == "dynamic_descriptors")
    return chaos_.BeforeDescriptorBind();
  return chaos_.BeforeGpuSubmit();
}

Result<SoakReport> SoakRunner::Run(
    const std::string& title_id, const std::string& version,
    const std::vector<CorpusCase>& cases, const CorpusRunner::Harness& harness,
    const gpu::HostGpuCaps& caps) {
  if (!db_)
    return Result<SoakReport>::Fail(RuntimeError::InvalidArgument, "no db");
  if (!db_->Lookup(title_id, version).ok())
    return Result<SoakReport>::Fail(RuntimeError::InvalidArgument,
                                    "unknown title");
  CorpusRunner runner(caps);
  for (auto& c : cases) runner.Add(c);
  auto results = runner.Run([&](const CorpusCase& c) -> Result<std::string> {
    if (auto ch = ApplyChaos(c.feature); !ch.ok())
      return Result<std::string>::Fail(ch.error, "chaos:" + ch.detail);
    return harness(c);
  });
  SoakReport rep;
  rep.results = results;
  for (auto& r : results) {
    if (r.skipped)
      ++rep.skipped;
    else {
      ++rep.ran;
      if (r.passed)
        ++rep.passed;
      else
        ++rep.failed;
    }
  }
  auto filed =
      CorpusCompatBridge::FileFailures(*db_, title_id, version, results);
  rep.filed = filed.ok() ? filed.value : 0;
  return Result<SoakReport>::Ok(std::move(rep));
}

}  // namespace kyty::infra
