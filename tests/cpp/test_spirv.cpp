#include <cassert>
#include <cstdio>
#include "kyty/ShaderCore.h"
#include "kyty/SpirvBackend.h"

static uint32_t W(uint16_t op, uint8_t a0 = 0, uint8_t a1 = 0) {
  return (uint32_t(op) << 16) | (uint32_t(a0) << 8) | a1;
}

int main() {
  using namespace kyty;
  using namespace kyty::shader;
  using namespace kyty::shader::spirv;

  CapabilityRegistry caps;
  ShaderPipeline pipe(caps);

  // Straight-line ALU -> real words -> validator accepts.
  auto pr = pipe.Compile({W(2, 1, 2), W(3, 2, 3), W(1, 4, 5)}, "cs");
  assert(pr.ok());
  auto mod = SpirvEmitter::Emit(pr.value.ir, "cs");
  assert(mod.ok());
  assert(mod.value.words[0] == kMagic);
  assert(mod.value.bound > 4);
  assert(SpirvValidator::Validate(mod.value).ok());

  // Empty IR (all Nop) still yields a valid empty-body module.
  auto empty = pipe.Compile({W(0)}, "ps");
  assert(empty.ok());
  auto me = SpirvEmitter::Emit(empty.value.ir, "ps");
  assert(me.ok() && SpirvValidator::Validate(me.value).ok());

  // Capability-gated ops fail explicitly, never silently wrong.
  auto mimg = pipe.Compile({W(0x10, 0, 0)}, "ps");
  assert(mimg.ok());
  assert(!SpirvEmitter::Emit(mimg.value.ir, "ps").ok());

  // Validator rejects: bad magic, truncation, zero bound, missing entry.
  auto bad = mod.value.words;
  bad[0] = 0xDEADBEEF;
  assert(!SpirvValidator::Validate(bad.data(), bad.size()).ok());
  assert(!SpirvValidator::Validate(mod.value.words.data(), 6).ok());
  auto zb = mod.value.words;
  zb[3] = 0;
  assert(!SpirvValidator::Validate(zb.data(), zb.size()).ok());
  assert(!SpirvValidator::Validate(nullptr, 0).ok());
  // Forged oversized length runs past the end.
  auto ov = mod.value.words;
  ov[5] = (99 << 16) | kOpCapability;
  assert(!SpirvValidator::Validate(ov.data(), ov.size()).ok());

  std::puts("test_spirv OK");
  return 0;
}
