#pragma once
// Real ISA tables — README §12/§13/§14/§19.
// Plugs genuine GCN/RDNA classes into the CapabilityRegistry:
// MIMG (sample/gather/getresinfo/atomics), SALU wave ballot LaneMask,
// barriers, buffer/image atomics. Each entry carries default per-stage
// status so coverage is measurable per stage, not assumed.
#include <cstdint>
#include <string>
#include <vector>
#include "kyty/Compat.h"
#include "kyty/ShaderCore.h"

namespace kyty::shader {

// GCN/RDNA MIMG opcode numbers (image class, abbreviated real set).
enum class MimgOp : uint8_t {
  SLoad = 0x00,
  SSample = 0x10,      // SAMPLE
  SSampleL = 0x11,     // SAMPLE_L
  SGather = 0x12,      // GATHER4
  GetResInfo = 0x13,
  AtomicSwap = 0x20,
  AtomicAdd = 0x21,
  AtomicSMin = 0x22,
  AtomicUMin = 0x23,
  AtomicSMax = 0x24,
  AtomicUMax = 0x25,
  AtomicAnd = 0x26,
  AtomicOr = 0x27,
  AtomicXor = 0x28,
  AtomicCmpSwap = 0x29,
};

// SALU / wavefront control (subset).
enum class SaluOp : uint8_t {
  SBallot = 0x01,   // ballot -> VCC/EXEC mask
  SAndSaveExec = 0x02,
  SWaveBarrier = 0x03,  // divergent-risk barrier
};

struct IsaEntry {
  std::string name;     // "MIMG.SAMPLE", "SALU.S_BARRIER", ...
  std::string cls;      // "mimg", "salu", "atomic", "barrier"
  uint32_t key = 0;     // toy-word opcode used by ShaderPipeline
  Op pipe_op = Op::Nop; // mapping into pipeline Op
  CapabilityEntry caps; // default per-stage status
};

class IsaTable {
 public:
  IsaTable();
  [[nodiscard]] const std::vector<IsaEntry>& entries() const {
    return entries_;
  }
  const IsaEntry* FindByKey(uint32_t key) const;
  const IsaEntry* FindByName(const std::string& name) const;
  // Installs table defaults into a registry (overrides toy defaults).
  void Install(CapabilityRegistry& reg) const;
  // Coverage: fraction of entries Covered/Lowered at a stage.
  [[nodiscard]] double CoverageAt(Stage stage) const;

 private:
  std::vector<IsaEntry> entries_;
};

}  // namespace kyty::shader
