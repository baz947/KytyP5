#pragma once
// Corpus <-> CompatDB bridge — README §24 (P1).
// A failing corpus case files a KnownIssue into the CompatDB under the
// subsystem owning that feature; AffectedBy then returns titles AND
// corpus evidence together. Fixes benefit every title sharing the
// requirement instead of per-game patches.
#include <string>
#include <vector>
#include "kyty/Compat.h"
#include "kyty/CompatDb.h"
#include "kyty/CorpusRunner.h"

namespace kyty::infra {

class CorpusCompatBridge {
 public:
  // Feature -> owning subsystem (matches KnownIssue.subsystem vocabulary).
  static std::string SubsystemFor(const std::string& feature);
  // File one issue per failed (non-skipped) corpus result.
  static Result<size_t> FileFailures(CompatDatabase& db,
                                     const std::string& title_id,
                                     const std::string& version,
                                     const std::vector<CorpusResult>& results);
  // Combined evidence: titles with issues in subsystem + corpus cases.
  struct Evidence {
    std::vector<TitleEntry> titles;
    std::vector<CorpusCase> cases;
  };
  static Evidence EvidenceFor(CompatDatabase& db,
                              const std::vector<CorpusCase>& corpus,
                              const std::string& subsystem);
};

}  // namespace kyty::infra
