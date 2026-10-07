#include <algorithm>
#include <cassert>
#include <cstdio>
#include "kyty/CompatDb.h"
#include "kyty/DescriptorDecoder.h"
#include "kyty/ElfLoader.h"
#include "kyty/FuzzChaos.h"
#include "kyty/ShaderCore.h"
#include "kyty/TraceDiagnostics.h"

int main() {
  using namespace kyty;
  using namespace kyty::infra;

  // Compat DB: upsert/lookup/affected-by/score.
  CompatDatabase db;
  TitleEntry e;
  e.title_id = "PPSA00001";
  e.version = "01.00";
  e.status = TitleStatus::Ingame;
  e.requirements.wave64 = true;
  e.issues.push_back(KnownIssue{"resource_tracking", CompatStatus::Broken, ""});
  assert(db.Upsert(e).ok());
  assert(db.Lookup("PPSA00001", "01.00").ok());
  assert(!db.Lookup("PPSA00001", "02.00").ok());
  assert(db.AffectedBy("resource_tracking").size() == 1);
  assert(db.AffectedBy("audio").empty());
  CompatScore s;
  s.dims = {{"Boot", 100}, {"Menu", 90}, {"Gameplay", 40}};
  assert(db.SetScore("PPSA00001", "01.00", s).ok());
  assert(db.Score("PPSA00001", "01.00").ok().value.Overall() == 76);

  // Trace: cross-subsystem slice by frame+submit.
  TraceCollector tc;
  Correlation c1;
  c1.frame = 7;
  c1.gpu_submit = 42;
  Correlation c2 = c1;
  c2.gpu_submit = 43;
  tc.Emit(TraceEvent{c1, "memory", "map", RuntimeError::None, 0x1000, "app"});
  tc.Emit(TraceEvent{c1, "gpu", "submit", RuntimeError::None, 0x1000, "app"});
  tc.Emit(TraceEvent{c2, "gpu", "other", RuntimeError::None, 0, ""});
  assert(tc.Slice(7, 42).size() == 2);
  assert(tc.Slice(7, 43).size() == 1);

  // Fuzz: deterministic seeds reproduce; loader/descriptor never crash.
  auto f1 = Fuzzer::MutateElf(1234);
  auto f2 = Fuzzer::MutateElf(1234);
  assert(f1.bytes == f2.bytes);
  assert(Fuzzer::MutateElf(999).bytes != f1.bytes);
  auto d = Fuzzer::MutateDescriptor(7);
  assert(d.bytes.size() == 64);
  // Feed fuzz bytes into real harnesses: must return structured errors.
  auto pe = kyty::linker::ElfLoader::ParseImage(d.bytes.data(),
                                                d.bytes.size());
  (void)pe;  // ok or structured fail, never throw (no-throw by contract)
  kyty::gpu::RawDescriptor rd;
  std::copy(d.bytes.begin(), d.bytes.end(), rd.bytes.begin());
  kyty::gpu::DescriptorDecoder dd;
  (void)dd.DecodeImage(rd, 1);
  auto sw = Fuzzer::MutateShaderWords(11, 4);
  assert(sw.bytes.size() == 16);

  // Chaos: armed faults -> structured errors; empty plan -> Ok.
  ChaosInjector calm(ChaosPlan{});
  assert(calm.BeforeGpuSubmit().ok());
  ChaosInjector storm(ChaosPlan{{ChaosFault::DeviceLost}});
  assert(!storm.BeforeGpuSubmit().ok());
  ChaosInjector memf(ChaosPlan{{ChaosFault::MemoryFault}});
  assert(!memf.BeforeMemAccess().ok());
  ChaosInjector miss(ChaosPlan{{ChaosFault::DescriptorMiss}});
  assert(!miss.BeforeDescriptorBind().ok());

  std::puts("test_infra OK");
  return 0;
}
