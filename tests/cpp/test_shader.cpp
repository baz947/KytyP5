#include <cassert>
#include <cstdio>
#include "kyty/ShaderCore.h"

static uint32_t W(uint16_t op, uint8_t a0 = 0, uint8_t a1 = 0) {
  return (uint32_t(op) << 16) | (uint32_t(a0) << 8) | a1;
}

int main() {
  using namespace kyty;
  using namespace kyty::shader;

  CapabilityRegistry caps;
  // Decode coverage != backend coverage: mark MIMG backend lowered.
  assert(caps.Get(Op::SampleMimg, Stage::Decode) == StageStatus::Covered);
  assert(caps.Get(Op::SampleMimg, Stage::Backend) == StageStatus::Lowered);
  assert(caps.Get(Op::Unknown, Stage::Decode) == StageStatus::Missing);

  // Straight-line compile: Mov/Add/Mul fold + bindings + fingerprint.
  ShaderPipeline pipe(caps);
  auto r = pipe.Compile({W(1, 5, 5), W(2, 1, 2), W(3, 1, 2)}, "ps");
  assert(r.ok());
  assert(r.value.info.hash != 0 && r.value.spirv_fingerprint != 0);
  assert(r.value.cfg.blocks.size() == 1);
  assert(IrBuilder::DeadCount(r.value.ir) == 1);  // Mov 5,5 folded
  assert(r.value.stage_status[uint8_t(Stage::Decode)] ==
         StageStatus::Covered);

  // CFG: cond branch creates 3 blocks with 3 edges.
  auto cfg = pipe.Compile({W(1, 1, 2), W(0x41, 3), W(2, 1, 1), W(0x42)}, "vs");
  assert(cfg.ok());
  assert(cfg.value.cfg.blocks.size() == 3);
  assert(cfg.value.cfg.EdgeCount() == 3);

  // Unknown op -> structured UnsupportedFeature (not crash).
  auto bad = pipe.Compile({0xFFFF0000}, "ps");
  assert(!bad.ok() && bad.error == RuntimeError::UnsupportedFeature);

  // Per-stage override: break backend only for Add.
  CapabilityRegistry caps2;
  caps2.Set(Op::Add, Stage::Backend, StageStatus::Missing);
  ShaderPipeline pipe2(caps2);
  auto r2 = pipe2.Compile({W(2, 1, 2)}, "cs");
  assert(r2.ok());
  assert(r2.value.stage_status[uint8_t(Stage::Backend)] ==
         StageStatus::Missing);
  assert(r2.value.stage_status[uint8_t(Stage::Decode)] ==
         StageStatus::Covered);

  // MIMG + atomics counted in shader info.
  auto m = pipe.Compile({W(0x10, 0, 0), W(0x31, 0, 0)}, "ps");
  assert(m.ok() && m.value.info.image_slots == 1);
  assert(m.value.info.uses_atomics);
  assert(!m.value.bindings.empty());

  std::puts("test_shader OK");
  return 0;
}
