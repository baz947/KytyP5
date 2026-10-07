#include <cassert>
#include <cstdio>
#include "kyty/CorpusRunner.h"
#include "kyty/HostGpuCaps.h"
#include "kyty/IsaTable.h"
#include "kyty/ShaderCore.h"

int main() {
  using namespace kyty;
  using namespace kyty::shader;
  using namespace kyty::infra;

  // ISA table: real entries, per-stage coverage measurable.
  IsaTable isa;
  assert(isa.entries().size() >= 10);
  assert(isa.FindByName("MIMG.SAMPLE") != nullptr);
  assert(isa.FindByName("SALU.S_BARRIER") != nullptr);
  assert(isa.CoverageAt(Stage::Decode) == 1.0);
  assert(isa.CoverageAt(Stage::Backend) > 0.9);

  // Install: MIMG.GATHER backend lowered propagates to pipeline.
  CapabilityRegistry reg;
  isa.Install(reg);
  assert(reg.Get(Op::SampleMimg, Stage::Backend) == StageStatus::Lowered ||
         reg.Get(Op::SampleMimg, Stage::Backend) == StageStatus::Covered);

  // Corpus: full-cap host runs everything; ray-less host skips rt.query.
  gpu::HostGpuCaps full(gpu::HostFeatures{});
  CorpusRunner run_full(full);
  for (auto& c : BuiltinCorpus()) run_full.Add(c);
  assert(run_full.Size() == 12);
  auto echo = [](const CorpusCase& c) -> Result<std::string> {
    return Result<std::string>::Ok(c.expected);  // oracle harness
  };
  auto res = run_full.Run(echo);
  size_t passed = 0, skipped = 0;
  for (auto& r : res) {
    passed += r.passed ? 1 : 0;
    skipped += r.skipped ? 1 : 0;
  }
  // Default HostFeatures has ray_query=false -> rt.query skipped.
  assert(skipped == 1 && passed == 11);

  // Differential: wrong harness output produces explicit diff, not crash.
  auto wrong = [](const CorpusCase& c) -> Result<std::string> {
    (void)c;
    return Result<std::string>::Ok("mismatch");
  };
  CorpusRunner run_one(full);
  run_one.Add(CorpusCase{"bda", "bda.resolve_ok", "Bda", "bda:valid",
                         "addr=ok"});
  auto d = run_one.Run(wrong);
  assert(!d[0].passed && !d[0].diff.empty());

  // Requirement gate: empty-cap host skips gated cases deterministically.
  gpu::HostFeatures bare{};
  bare.descriptor_arrays = false;
  bare.bda = false;
  bare.image_atomics = false;
  bare.subgroup32 = false;
  CorpusRunner run_bare(gpu::HostGpuCaps(bare));
  for (auto& c : BuiltinCorpus()) run_bare.Add(c);
  auto rb = run_bare.Run(echo);
  size_t sk2 = 0;
  bool rt_skipped = false;
  for (auto& r : rb) {
    sk2 += r.skipped ? 1 : 0;
    if (r.name == "rt.query" && r.skipped) rt_skipped = true;
  }
  // Only Unavailable gates skip; Emulated/Fallback still run via lowering.
  assert(sk2 >= 1 && rt_skipped);

  std::puts("test_isa_corpus OK");
  return 0;
}
