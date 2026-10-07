#pragma once
// Shader core — README §12 (P0).
// Pipeline: Decode -> CFG -> Translate -> IR -> SSA/simplify -> DCE/
// read-lane/tess lowering -> Resource Tracking -> Specialize/Materialize ->
// Shader Info -> Binding Alloc -> SPIR-V (stub emits fingerprint).
// Rule: Decoder coverage != CFG coverage != Translator != IR != Backend.
// Instruction capability registry tracks status per stage.
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include "kyty/Compat.h"

namespace kyty::shader {

enum class Stage : uint8_t { Decode = 0, Cfg, Translate, Ir, Backend };
enum class StageStatus : uint8_t {
  Covered = 0,   // exact
  Lowered,       // compatible via lowering
  Emulated,
  Missing,       // unsupported at this stage
  Broken,
};

// Toy ISA opcodes (explicit, testable; real GCN/RDNA tables plug in here).
enum class Op : uint16_t {
  Nop = 0,
  Mov = 1,
  Add = 2,
  Mul = 3,
  SampleMimg = 0x10,  // MIMG (§13)
  ReadLane = 0x20,    // wave ops (§14)
  WriteLane = 0x21,
  Broadcast = 0x22,
  Barrier = 0x30,     // §19
  AtomicBuf = 0x31,
  Branch = 0x40,      // CFG
  BranchCond = 0x41,
  Return = 0x42,
  Unknown = 0xFFFF,
};

struct RawInst {
  Op op = Op::Nop;
  uint32_t arg0 = 0, arg1 = 0;
};

struct CapabilityEntry {
  StageStatus decode = StageStatus::Covered;
  StageStatus cfg = StageStatus::Covered;
  StageStatus translate = StageStatus::Covered;
  StageStatus ir = StageStatus::Covered;
  StageStatus backend = StageStatus::Covered;
};

class CapabilityRegistry {
 public:
  CapabilityRegistry();
  void Set(Op op, Stage stage, StageStatus s);
  [[nodiscard]] StageStatus Get(Op op, Stage stage) const;
  [[nodiscard]] bool SupportedAt(Op op, Stage stage) const;

 private:
  std::map<uint16_t, CapabilityEntry> table_;
};

// ---- CFG ----
struct BasicBlock {
  uint32_t id = 0;
  size_t begin = 0, end = 0;  // [begin,end) instruction indices
  std::vector<uint32_t> succ;
};

struct Cfg {
  std::vector<BasicBlock> blocks;
  [[nodiscard]] size_t EdgeCount() const;
};

class CfgBuilder {
 public:
  // Builds CFG honoring Branch/BranchCond/Return; Unknown op -> Missing.
  static Result<Cfg> Build(const std::vector<RawInst>& insts,
                           const CapabilityRegistry& caps);
};

// ---- IR ----
enum class IrOp : uint8_t {
  Copy = 0, Add, Mul,
  ImageSample,       // from SampleMimg
  ReadLane, WriteLane, Broadcast, Shuffle, Ballot, ActiveMask,
  Barrier, Atomic,
  Phi,               // SSA merge
  Dead,              // DCE marker
};

struct IrInst {
  uint32_t id = 0;
  IrOp op = IrOp::Copy;
  std::vector<uint32_t> args;
  bool lowered_tess = false;  // tessellation lowering flag
};

struct IrModule {
  std::vector<IrInst> insts;
  uint32_t next_id = 1;
};

class IrBuilder {
 public:
  // Translate raw -> IR; per-op capability checked at Translate stage.
  // Runs simplify (fold Mov chains) + DCE + read-lane/tess lowering notes.
  static Result<IrModule> Translate(const std::vector<RawInst>& insts,
                                    const CapabilityRegistry& caps);
  static size_t DeadCount(const IrModule& m);
};

// ---- Shader info + bindings + pipeline ----
struct ShaderInfo {
  uint64_t hash = 0;
  std::string stage;  // "vs","ps","cs",...
  uint32_t vgpr = 0, sgpr = 0;
  uint32_t image_slots = 0, buffer_slots = 0;
  bool uses_wave64 = false;
  bool uses_mimg_compare = false;
  bool uses_atomics = false;
};

struct Binding {
  uint32_t set = 0, binding = 0;
  std::string kind;  // "image","buffer","sampler"
};

class BindingAllocator {
 public:
  static std::vector<Binding> Allocate(const ShaderInfo& info,
                                       bool has_update_after_bind);
};

struct PipelineResult {
  ShaderInfo info;
  IrModule ir;
  Cfg cfg;
  std::vector<Binding> bindings;
  uint64_t spirv_fingerprint = 0;  // stub backend emits hash, not real SPIR-V
  std::map<uint8_t, StageStatus> stage_status;  // worst per stage
};

class ShaderPipeline {
 public:
  explicit ShaderPipeline(CapabilityRegistry caps = {}) : caps_(caps) {}
  // Decodes u32 words: [op:16][arg0:8][arg1:8] (explicit test encoding).
  static Result<std::vector<RawInst>> DecodeWords(
      const std::vector<uint32_t>& words, const CapabilityRegistry& caps);
  Result<PipelineResult> Compile(const std::vector<uint32_t>& words,
                                 const std::string& stage) const;

 private:
  CapabilityRegistry caps_;
};

}  // namespace kyty::shader
