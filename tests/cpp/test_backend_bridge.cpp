#include <cassert>
#include <cstdio>
#include "kyty/BackendLowering.h"
#include "kyty/CorpusCompatBridge.h"
#include "kyty/CorpusRunner.h"
#include "kyty/HostGpuCaps.h"

int main() {
  using namespace kyty;
  using namespace kyty::shader;
  using namespace kyty::infra;

  // Backend lowering: native host -> None; wave64 w/o sub64 -> Split.
  gpu::HostGpuCaps full(gpu::HostFeatures{});
  ShaderInfo wave;
  wave.hash = 0xABCD;
  wave.uses_wave64 = true;
  gpu::HostFeatures no64 = gpu::HostFeatures{};
  no64.subgroup64 = false;  // default already false -> Lowered
  auto lw = BackendLowering::Lower(wave, gpu::HostGpuCaps(no64), 1, 1);
  assert(lw.ok() && lw.value.kind == LoweringKind::Wave64Split);
  assert(lw.value.passes.size() == 2);
  assert(!lw.value.pipeline_key.empty());

  // Native wave64 host -> no lowering, key differs from split key.
  gpu::HostFeatures has64 = gpu::HostFeatures{};
  has64.subgroup64 = true;
  auto ln = BackendLowering::Lower(wave, gpu::HostGpuCaps(has64), 1, 1);
  assert(ln.ok() && ln.value.kind == LoweringKind::None);
  assert(ln.value.pipeline_key != lw.value.pipeline_key);

  // MIMG compare -> explicit lowering pass.
  ShaderInfo cmp;
  cmp.hash = 0x1234;
  cmp.uses_mimg_compare = true;
  auto lc = BackendLowering::Lower(cmp, full, 1, 1);
  assert(lc.ok() && lc.value.kind == LoweringKind::MimgCompare);

  // Bridge: subsystem mapping + failure filing + combined evidence.
  assert(CorpusCompatBridge::SubsystemFor("wave64") == "wave_model");
  assert(CorpusCompatBridge::SubsystemFor("bda") == "guest_memory");
  assert(CorpusCompatBridge::SubsystemFor("ray_tracing") == "host_caps");

  CompatDatabase db;
  TitleEntry e;
  e.title_id = "PPSA00001";
  e.version = "01.00";
  assert(db.Upsert(e).ok());
  CorpusRunner run(full);
  run.Add(CorpusCase{"bda", "bda.resolve_ok", "Bda", "bda:valid", "addr=ok"});
  auto wrong = [](const CorpusCase&) -> Result<std::string> {
    return Result<std::string>::Ok("mismatch");
  };
  auto res = run.Run(wrong);
  assert(!res[0].passed);
  auto filed = CorpusCompatBridge::FileFailures(db, "PPSA00001", "01.00", res);
  assert(filed.ok() && filed.value == 1);
  auto t = db.Lookup("PPSA00001", "01.00");
  assert(t.ok() && t.value.issues.size() == 1);
  assert(t.value.issues[0].subsystem == "guest_memory");

  auto ev = CorpusCompatBridge::EvidenceFor(db, BuiltinCorpus(), "guest_memory");
  assert(!ev.titles.empty() && !ev.cases.empty());

  // Unknown title -> structured MissingDependency-style error.
  auto bad =
      CorpusCompatBridge::FileFailures(db, "GHOST", "00.00", res);
  assert(!bad.ok());

  std::puts("test_backend_bridge OK");
  return 0;
}
