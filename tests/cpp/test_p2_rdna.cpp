#include <cassert>
#include <cstdio>
#include "kyty/GuestClock.h"
#include "kyty/P2PerfPresent.h"
#include "kyty/RdnaIsaFull.h"

int main() {
  using namespace kyty;
  using namespace kyty::shader;
  using namespace kyty::p2;

  // Full RDNA tables: size, key lookups, per-stage coverage.
  RdnaIsaFull full;
  assert(full.entries().size() >= 50);
  assert(full.FindByName("MIMG.SAMPLE_P") != nullptr);
  assert(full.FindByName("MIMG.ATOMIC_CMPXCHG") != nullptr);
  assert(full.FindByName("DS.ADD_U32") != nullptr);
  assert(full.FindByName("FLAT.LOAD") != nullptr);
  assert(full.FindByName("VOP2.MAD_F32") != nullptr);
  // #281 families covered as first-class entries (not lumped).
  assert(full.FindByName("SOP1.S_MOV_B32") != nullptr);
  assert(full.FindByName("SOPC.S_CMP_EQ_U32") != nullptr);
  assert(full.FindByName("VOP3P.V_PK_ADD_F16") != nullptr);
  assert(full.FindByName("VINTRP.V_INTERP_P1_F32") != nullptr);
  assert(full.FindByName("EXP.EXPORT") != nullptr);
  assert(full.CoverageAt(Stage::Decode) == 1.0);
  assert(full.CoverageAt(Stage::Backend) > 0.9);
  auto by_cls = full.CoverageByClass(Stage::Backend);
  assert(!by_cls.empty());
  for (auto& [cls, cov] : by_cls) assert(cov > 0.8);

  // Install full tables keeps pipeline decodable (class-mapped keys).
  CapabilityRegistry reg;
  full.InstallAll(reg);
  assert(reg.Get(Op::SampleMimg, Stage::Decode) == StageStatus::Covered);

  // Pipeline cache: hit/miss/evict LRU.
  PipelineCache pc(2);
  assert(!pc.Lookup("a").has_value());
  pc.Store("a", 1);
  pc.Store("b", 2);
  assert(pc.Lookup("a").value() == 1);  // a now MRU
  pc.Store("c", 3);                    // evicts b (LRU)
  assert(!pc.Lookup("b").has_value());
  assert(pc.Lookup("c").value() == 3);
  auto st = pc.Stats();
  assert(st.hits >= 2 && st.misses >= 2 && st.evictions == 1);

  // Present scheduler: FIFO pacing + mailbox drops + VRR gating.
  GuestClock clk;
  PresentScheduler fifo(&clk, true, true);
  PresentConfig cfg;
  cfg.mode = PresentMode::Fifo;
  cfg.target_fps = 60;
  assert(fifo.Configure(cfg).ok());
  assert(fifo.SubmitFrame(101).ok());
  assert(!fifo.SubmitFrame(0).ok());
  clk.AdvanceNs(20'000'000);
  auto p = fifo.PresentDue();
  assert(p.size() == 1 && p[0] == 101);
  assert(fifo.Stats().presented == 1);

  // Mailbox drops superseded frames under pressure.
  PresentScheduler mb(&clk, true, false);
  PresentConfig mcg;
  mcg.mode = PresentMode::Mailbox;
  mcg.queue_depth = 2;
  assert(mb.Configure(mcg).ok());
  assert(mb.SubmitFrame(1).ok());
  assert(mb.SubmitFrame(2).ok());
  assert(mb.SubmitFrame(3).ok());  // depth overflow -> 1 dropped
  assert(mb.Stats().dropped == 1);
  clk.AdvanceNs(20'000'000);
  auto pm = mb.PresentDue();
  assert(pm.size() == 1);  // newest only

  // VRR/HDR without host support -> structured UnsupportedFeature.
  PresentScheduler nohost(&clk, false, false);
  PresentConfig vrr = cfg;
  vrr.vrr = true;
  assert(!nohost.Configure(vrr).ok());
  PresentConfig hdr = cfg;
  hdr.hdr = true;
  assert(!nohost.Configure(hdr).ok());

  // Long-run: stall counted on >2-period gap between presents.
  PresentScheduler lr(&clk, false, false);
  assert(lr.Configure(cfg).ok());
  assert(lr.SubmitFrame(7).ok());
  clk.AdvanceNs(20'000'000);
  (void)lr.PresentDue();  // baseline present
  assert(lr.SubmitFrame(8).ok());
  clk.AdvanceNs(100'000'000);  // ~6 periods at 60fps
  (void)lr.PresentDue();
  assert(lr.Stats().stalls == 1);

  std::puts("test_p2_rdna OK");
  return 0;
}
