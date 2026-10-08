#pragma once
// Soak runner — README §25 chaos mode (P1).
// Runs the feature corpus under an armed ChaosPlan end to end:
// each case consults the chaos hook owning its feature, the harness
// produces the semantic result, and failures are filed into the
// CompatDatabase via the bridge. Deterministic: same plan, same report.
#include <string>
#include <vector>
#include "kyty/Compat.h"
#include "kyty/CompatDb.h"
#include "kyty/CorpusRunner.h"
#include "kyty/FuzzChaos.h"

namespace kyty::infra {

struct SoakReport {
  size_t ran = 0;
  size_t passed = 0;
  size_t failed = 0;
  size_t skipped = 0;
  size_t filed = 0;
  std::vector<CorpusResult> results;
};

class SoakRunner {
 public:
  SoakRunner(CompatDatabase* db, ChaosInjector chaos)
      : db_(db), chaos_(std::move(chaos)) {}
  // Runs cases for (title, version); title must exist in db.
  Result<SoakReport> Run(const std::string& title_id,
                         const std::string& version,
                         const std::vector<CorpusCase>& cases,
                         const CorpusRunner::Harness& harness,
                         const gpu::HostGpuCaps& caps);

 private:
  // Feature -> owning chaos hook (mirrors CorpusCompatBridge subsystems).
  Result<void> ApplyChaos(const std::string& feature) const;
  CompatDatabase* db_ = nullptr;
  ChaosInjector chaos_;
};

}  // namespace kyty::infra
