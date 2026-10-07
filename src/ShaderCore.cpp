#include "kyty/ShaderCore.h"

namespace kyty::shader {

CapabilityRegistry::CapabilityRegistry() {
  // Defaults: common ALU/CFG covered everywhere; MIMG compare lowered at
  // backend; tessellation via lowering flag; Unknown missing everywhere.
  CapabilityEntry full;
  for (uint16_t o : {0, 1, 2, 3, 0x20, 0x21, 0x22, 0x30, 0x31, 0x40, 0x41,
                      0x42})
    table_[o] = full;
  CapabilityEntry mimg = full;
  mimg.backend = StageStatus::Lowered;
  table_[0x10] = mimg;
}

void CapabilityRegistry::Set(Op op, Stage stage, StageStatus s) {
  auto& e = table_[uint16_t(op)];
  switch (stage) {
    case Stage::Decode: e.decode = s; break;
    case Stage::Cfg: e.cfg = s; break;
    case Stage::Translate: e.translate = s; break;
    case Stage::Ir: e.ir = s; break;
    case Stage::Backend: e.backend = s; break;
  }
}

StageStatus CapabilityRegistry::Get(Op op, Stage stage) const {
  auto it = table_.find(uint16_t(op));
  if (it == table_.end()) return StageStatus::Missing;
  switch (stage) {
    case Stage::Decode: return it->second.decode;
    case Stage::Cfg: return it->second.cfg;
    case Stage::Translate: return it->second.translate;
    case Stage::Ir: return it->second.ir;
    case Stage::Backend: return it->second.backend;
  }
  return StageStatus::Missing;
}

bool CapabilityRegistry::SupportedAt(Op op, Stage stage) const {
  auto s = Get(op, stage);
  return s == StageStatus::Covered || s == StageStatus::Lowered ||
         s == StageStatus::Emulated;
}

size_t Cfg::EdgeCount() const {
  size_t n = 0;
  for (auto& b : blocks) n += b.succ.size();
  return n;
}

Result<Cfg> CfgBuilder::Build(const std::vector<RawInst>& insts,
                              const CapabilityRegistry& caps) {
  if (insts.empty())
    return Result<Cfg>::Fail(RuntimeError::InvalidArgument, "empty shader");
  // Leaders: 0 + after branch/cond/return + branch targets (arg0 index).
  std::vector<bool> leader(insts.size(), false);
  leader[0] = true;
  for (size_t i = 0; i < insts.size(); ++i) {
    Op op = insts[i].op;
    if (!caps.SupportedAt(op, Stage::Cfg))
      return Result<Cfg>::Fail(RuntimeError::UnsupportedFeature,
                               "CFG missing op");
    if ((op == Op::Branch || op == Op::BranchCond || op == Op::Return) &&
        i + 1 < insts.size())
      leader[i + 1] = true;
    if ((op == Op::Branch || op == Op::BranchCond) &&
        insts[i].arg0 < insts.size())
      leader[insts[i].arg0] = true;
  }
  Cfg cfg;
  uint32_t id = 0;
  size_t cur = 0;
  for (size_t i = 0; i <= insts.size(); ++i) {
    bool boundary = (i == insts.size()) || (i > cur && leader[i]);
    if (boundary && i > cur) {
      BasicBlock b{id++, cur, i, {}};
      Op last = insts[i - 1].op;
      if (last == Op::Branch) {
        b.succ.push_back(0);  // patched below by index->block map
      } else if (last == Op::BranchCond) {
        b.succ.push_back(0);
        b.succ.push_back(0);
      } else if (last != Op::Return) {
        b.succ.push_back(0);  // fallthrough placeholder
      }
      cfg.blocks.push_back(std::move(b));
      cur = i;
    }
  }
  // Resolve placeholders: map inst index -> block id.
  std::vector<uint32_t> idx2block(insts.size(), 0);
  for (auto& b : cfg.blocks) {
    for (size_t i = b.begin; i < b.end; ++i) idx2block[i] = b.id;
  }
  for (auto& b : cfg.blocks) {
    if (b.succ.empty()) continue;
    Op last = insts[b.end - 1].op;
    b.succ.clear();
    if (last == Op::Branch) {
      uint32_t t = insts[b.end - 1].arg0;
      if (t >= insts.size())
        return Result<Cfg>::Fail(RuntimeError::InvalidArgument,
                                 "bad branch target");
      b.succ.push_back(idx2block[t]);
    } else if (last == Op::BranchCond) {
      uint32_t t = insts[b.end - 1].arg0;
      if (t >= insts.size())
        return Result<Cfg>::Fail(RuntimeError::InvalidArgument,
                                 "bad cbranch target");
      b.succ.push_back(idx2block[t]);
      if (b.end < insts.size()) b.succ.push_back(idx2block[b.end]);
    } else {
      if (b.end < insts.size()) b.succ.push_back(idx2block[b.end]);
    }
  }
  return Result<Cfg>::Ok(std::move(cfg));
}

static IrOp TranslateOp(Op op) {
  switch (op) {
    case Op::Mov: return IrOp::Copy;
    case Op::Add: return IrOp::Add;
    case Op::Mul: return IrOp::Mul;
    case Op::SampleMimg: return IrOp::ImageSample;
    case Op::ReadLane: return IrOp::ReadLane;
    case Op::WriteLane: return IrOp::WriteLane;
    case Op::Broadcast: return IrOp::Broadcast;
    case Op::Barrier: return IrOp::Barrier;
    case Op::AtomicBuf: return IrOp::Atomic;
    default: return IrOp::Copy;
  }
}

Result<IrModule> IrBuilder::Translate(const std::vector<RawInst>& insts,
                                      const CapabilityRegistry& caps) {
  IrModule m;
  for (auto& in : insts) {
    if (in.op == Op::Branch || in.op == Op::BranchCond || in.op == Op::Return ||
        in.op == Op::Nop)
      continue;  // control flow lives in CFG, not IR body
    if (!caps.SupportedAt(in.op, Stage::Translate))
      return Result<IrModule>::Fail(RuntimeError::UnsupportedFeature,
                                    "translate missing op");
    IrInst ii;
    ii.id = m.next_id++;
    ii.op = TranslateOp(in.op);
    ii.args = {in.arg0, in.arg1};
    m.insts.push_back(ii);
  }
  // Simplify: fold Mov x,x (copy propagation stub) -> mark Dead.
  for (auto& ii : m.insts) {
    if (ii.op == IrOp::Copy && ii.args.size() == 2 &&
        ii.args[0] == ii.args[1])
      ii.op = IrOp::Dead;
  }
  return Result<IrModule>::Ok(std::move(m));
}

size_t IrBuilder::DeadCount(const IrModule& m) {
  size_t n = 0;
  for (auto& ii : m.insts)
    if (ii.op == IrOp::Dead) ++n;
  return n;
}

std::vector<Binding> BindingAllocator::Allocate(const ShaderInfo& info,
                                                bool has_update_after_bind) {
  std::vector<Binding> out;
  uint32_t b = 0;
  for (uint32_t i = 0; i < info.image_slots; ++i)
    out.push_back(Binding{has_update_after_bind ? 1u : 0u, b++, "image"});
  for (uint32_t i = 0; i < info.buffer_slots; ++i)
    out.push_back(Binding{has_update_after_bind ? 1u : 0u, b++, "buffer"});
  return out;
}

Result<std::vector<RawInst>> ShaderPipeline::DecodeWords(
    const std::vector<uint32_t>& words, const CapabilityRegistry& caps) {
  std::vector<RawInst> out;
  for (auto w : words) {
    Op op = Op(uint16_t(w >> 16));
    if (op == Op::Unknown || !caps.SupportedAt(op, Stage::Decode))
      return Result<std::vector<RawInst>>::Fail(
          RuntimeError::UnsupportedFeature, "decode missing op");
    out.push_back(
        RawInst{op, uint32_t((w >> 8) & 0xFF), uint32_t(w & 0xFF)});
  }
  return Result<std::vector<RawInst>>::Ok(std::move(out));
}

Result<PipelineResult> ShaderPipeline::Compile(
    const std::vector<uint32_t>& words, const std::string& stage) const {
  auto di = DecodeWords(words, caps_);
  if (!di.ok())
    return Result<PipelineResult>::Fail(di.error, di.detail);
  auto cfg = CfgBuilder::Build(di.value, caps_);
  if (!cfg.ok())
    return Result<PipelineResult>::Fail(cfg.error, cfg.detail);
  auto ir = IrBuilder::Translate(di.value, caps_);
  if (!ir.ok())
    return Result<PipelineResult>::Fail(ir.error, ir.detail);

  PipelineResult pr;
  pr.cfg = cfg.value;
  pr.ir = ir.value;
  pr.info.stage = stage;
  uint64_t h = 1469598103934665603ull;
  for (auto w : words) {
    h ^= w;
    h *= 1099511628211ull;
  }
  pr.info.hash = h;
  for (auto& ii : pr.ir.insts) {
    if (ii.op == IrOp::ImageSample) {
      pr.info.image_slots++;
      if (ii.args.size() > 1 && (ii.args[1] & 0x4))  // compare flag
        pr.info.uses_mimg_compare = true;
    }
    if (ii.op == IrOp::Atomic) {
      pr.info.buffer_slots++;
      pr.info.uses_atomics = true;
    }
    if (ii.op == IrOp::ReadLane || ii.op == IrOp::Broadcast)
      pr.info.uses_wave64 = false;  // sized later by WaveModel
  }
  pr.info.vgpr = uint32_t(pr.ir.insts.size() * 2);
  pr.bindings = BindingAllocator::Allocate(pr.info, true);
  pr.spirv_fingerprint = h ^ 0x9E3779B97F4A7C15ull;
  // Worst status per stage across ops (explicit coverage accounting).
  for (uint8_t s = 0; s <= uint8_t(Stage::Backend); ++s) {
    StageStatus worst = StageStatus::Covered;
    for (auto& in : di.value) {
      auto cur = caps_.Get(in.op, Stage(s));
      if (cur == StageStatus::Missing || cur == StageStatus::Broken) {
        worst = cur;
        break;
      }
      if (cur == StageStatus::Emulated) worst = cur;
      if (cur == StageStatus::Lowered && worst == StageStatus::Covered)
        worst = cur;
    }
    pr.stage_status[s] = worst;
  }
  return Result<PipelineResult>::Ok(std::move(pr));
}

}  // namespace kyty::shader
