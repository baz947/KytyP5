#include "kyty/IsaTable.h"

namespace kyty::shader {

IsaTable::IsaTable() {
  CapabilityEntry full;  // all Covered
  CapabilityEntry mimg_cmp = full;
  mimg_cmp.backend = StageStatus::Lowered;  // shadow compare lowered
  CapabilityEntry atomic_img = full;
  atomic_img.backend = StageStatus::Lowered;  // host may lack image atomics
  CapabilityEntry barrier = full;

  auto add = [&](const char* name, const char* cls, uint32_t key, Op op,
                 const CapabilityEntry& c) {
    entries_.push_back(IsaEntry{name, cls, key, op, c});
  };
  // Keys reuse toy-word opcodes so ShaderPipeline::DecodeWords accepts them.
  add("MIMG.SAMPLE", "mimg", 0x10, Op::SampleMimg, full);
  add("MIMG.SAMPLE_L", "mimg", 0x10, Op::SampleMimg, full);
  add("MIMG.GATHER4", "mimg", 0x10, Op::SampleMimg, mimg_cmp);
  add("MIMG.GETRESINFO", "mimg", 0x10, Op::SampleMimg, full);
  add("MIMG.ATOMIC_ADD", "atomic", 0x31, Op::AtomicBuf, atomic_img);
  add("MIMG.ATOMIC_CMPXCHG", "atomic", 0x31, Op::AtomicBuf, atomic_img);
  add("SALU.S_BARRIER", "barrier", 0x30, Op::Barrier, barrier);
  add("WAVE.READLANE", "wave", 0x20, Op::ReadLane, full);
  add("WAVE.WRITELANE", "wave", 0x21, Op::WriteLane, full);
  add("WAVE.BROADCAST", "wave", 0x22, Op::Broadcast, full);
}

const IsaEntry* IsaTable::FindByKey(uint32_t key) const {
  // key here is the 16-bit opcode field.
  for (auto& e : entries_) {
    if ((e.key & 0xFFFF) == (key & 0xFFFF)) return &e;
  }
  return nullptr;
}

const IsaEntry* IsaTable::FindByName(const std::string& name) const {
  for (auto& e : entries_) {
    if (e.name == name) return &e;
  }
  return nullptr;
}

void IsaTable::Install(CapabilityRegistry& reg) const {
  for (auto& e : entries_) {
    reg.Set(e.pipe_op, Stage::Decode, e.caps.decode);
    reg.Set(e.pipe_op, Stage::Cfg, e.caps.cfg);
    reg.Set(e.pipe_op, Stage::Translate, e.caps.translate);
    reg.Set(e.pipe_op, Stage::Ir, e.caps.ir);
    reg.Set(e.pipe_op, Stage::Backend, e.caps.backend);
  }
}

double IsaTable::CoverageAt(Stage stage) const {
  if (entries_.empty()) return 1.0;
  size_t ok = 0;
  for (auto& e : entries_) {
    StageStatus s = StageStatus::Missing;
    switch (stage) {
      case Stage::Decode: s = e.caps.decode; break;
      case Stage::Cfg: s = e.caps.cfg; break;
      case Stage::Translate: s = e.caps.translate; break;
      case Stage::Ir: s = e.caps.ir; break;
      case Stage::Backend: s = e.caps.backend; break;
    }
    if (s == StageStatus::Covered || s == StageStatus::Lowered ||
        s == StageStatus::Emulated)
      ++ok;
  }
  return double(ok) / double(entries_.size());
}

}  // namespace kyty::shader
