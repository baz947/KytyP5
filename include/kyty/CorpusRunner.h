#pragma once
// Feature corpus + differential harness — README §25.
// Corpus organized by feature (not game): wave64/, dynamic_descriptors/,
// image_formats/, depth_stencil/, bda/, atomics/, barriers/, videoout/,
// savedata/, tls/, network/. Each manifest declares requirements;
// the runner skips entries the host caps cannot satisfy and diffs
// guest-expected vs Kyty-actual semantically.
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include "kyty/Compat.h"
#include "kyty/HostGpuCaps.h"

namespace kyty::infra {

struct CorpusCase {
  std::string feature;   // "wave64", "bda", ...
  std::string name;      // "wave64.split_basic"
  std::string requirement;  // "Wave64", "Bda", "DynamicDescriptors", ...
  std::string input;     // guest input descriptor (opaque to runner)
  std::string expected;  // expected semantic result fingerprint
};

struct CorpusResult {
  std::string name;
  bool skipped = false;
  bool passed = false;
  std::string actual;
  std::string diff;
};

class CorpusRunner {
 public:
  using Harness = std::function<Result<std::string>(const CorpusCase&)>;
  explicit CorpusRunner(gpu::HostGpuCaps caps) : caps_(caps) {}
  void Add(CorpusCase c) { cases_.push_back(std::move(c)); }
  // Runs all cases through harness; requirement unmet -> skipped.
  std::vector<CorpusResult> Run(const Harness& h) const;
  [[nodiscard]] size_t Size() const { return cases_.size(); }

 private:
  bool RequirementMet(const std::string& req) const;
  gpu::HostGpuCaps caps_;
  std::vector<CorpusCase> cases_;
};

// Built-in manifests (mirrors tests/corpus/<feature>/manifest).
std::vector<CorpusCase> BuiltinCorpus();

}  // namespace kyty::infra
