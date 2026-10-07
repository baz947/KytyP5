#include "kyty/RdnaIsaFull.h"
#include <map>

namespace kyty::shader {

RdnaIsaFull::RdnaIsaFull() {
  CapabilityEntry full;
  CapabilityEntry lowered = full;
  lowered.backend = StageStatus::Lowered;
  CapabilityEntry emu = full;
  emu.backend = StageStatus::Emulated;

  auto add = [&](const char* name, const char* cls, uint32_t key, Op op,
                 const CapabilityEntry& c) {
    entries_.push_back(IsaEntry{name, cls, key, op, c});
  };
  // ---- MIMG: sample/load/store/atomics (pipe key 0x10 / 0x31) ----
  const char* samples[] = {"MIMG.SAMPLE",      "MIMG.SAMPLE_P",
                           "MIMG.SAMPLE_L",    "MIMG.SAMPLE_LB",
                           "MIMG.SAMPLE_GATHER4", "MIMG.GETRESINFO",
                           "MIMG.LD",          "MIMG.ST"};
  for (auto n : samples) {
    bool cmp = std::string(n).find("GATHER") != std::string::npos;
    add(n, "mimg", 0x10, Op::SampleMimg, cmp ? lowered : full);
  }
  const char* mimg_atom[] = {
      "MIMG.ATOMIC_SWAP", "MIMG.ATOMIC_ADD",   "MIMG.ATOMIC_SMIN",
      "MIMG.ATOMIC_UMIN", "MIMG.ATOMIC_SMAX",  "MIMG.ATOMIC_UMAX",
      "MIMG.ATOMIC_AND",  "MIMG.ATOMIC_OR",    "MIMG.ATOMIC_XOR",
      "MIMG.ATOMIC_INC",  "MIMG.ATOMIC_DEC",   "MIMG.ATOMIC_CMPXCHG"};
  for (auto n : mimg_atom) add(n, "mimg-atomic", 0x31, Op::AtomicBuf, lowered);
  // ---- MTBUF/MUBUF buffer ops (key 0x31 / mov) ----
  const char* bufs[] = {"MTBUF.LOAD_FORMAT", "MTBUF.STORE_FORMAT",
                        "MUBUF.LOAD", "MUBUF.STORE", "MUBUF.ATOMIC_ADD",
                        "MUBUF.ATOMIC_CMPXCHG"};
  for (auto n : bufs) {
    bool atom = std::string(n).find("ATOMIC") != std::string::npos;
    add(n, "buffer", 0x31, atom ? Op::AtomicBuf : Op::Mov, atom ? lowered : full);
  }
  // ---- SMEM ----
  add("SMEM.LOAD", "smem", 1, Op::Mov, full);
  add("SMEM.STORE", "smem", 1, Op::Mov, full);
  // ---- SOPP / SALU wave control ----
  add("SOPP.S_BARRIER", "barrier", 0x30, Op::Barrier, full);
  add("SOPP.S_NOP", "sopp", 0, Op::Nop, full);
  add("SALU.S_AND_SAVEEXEC", "salu", 0x22, Op::Broadcast, full);
  add("SALU.S_MOV_B64", "salu", 1, Op::Mov, full);
  add("SALU.S_BALLOT", "wave", 0x22, Op::Broadcast, full);
  // ---- VOP1/2/3 ALU ----
  const char* vops[] = {"VOP1.MOV", "VOP2.ADD_F32", "VOP2.MUL_F32",
                        "VOP2.MAD_F32", "VOP3.MAD_F64", "VOP1.FLOOR"};
  for (auto n : vops) {
    std::string s = n;
    Op op = s.find("ADD") != std::string::npos ? Op::Add
            : s.find("MUL") != std::string::npos || s.find("MAD") != std::string::npos
                ? Op::Mul
                : Op::Mov;
    add(n, "vop", op == Op::Add ? 2 : op == Op::Mul ? 3 : 1, op, full);
  }
  // ---- VOPC compares ----
  add("VOPC.CMP_EQ_F32", "vopc", 2, Op::Add, full);
  add("VOPC.CMP_LT_F32", "vopc", 2, Op::Add, full);
  // ---- DS (LDS) ----
  const char* dsops[] = {"DS.ADD_U32", "DS.XCHG", "DS.CMPST",
                         "DS.BARRIER"};
  for (auto n : dsops) {
    bool bar = std::string(n).find("BARRIER") != std::string::npos;
    add(n, "ds", bar ? 0x30 : 0x31, bar ? Op::Barrier : Op::AtomicBuf,
        bar ? full : lowered);
  }
  // ---- FLAT ----
  add("FLAT.LOAD", "flat", 1, Op::Mov, full);
  add("FLAT.STORE", "flat", 1, Op::Mov, full);
  add("FLAT.ATOMIC_ADD", "flat", 0x31, Op::AtomicBuf, lowered);
  // ---- Wave lane ops ----
  add("WAVE.READLANE", "wave", 0x20, Op::ReadLane, full);
  add("WAVE.WRITELANE", "wave", 0x21, Op::WriteLane, full);
  add("WAVE.BROADCAST", "wave", 0x22, Op::Broadcast, full);
  // ---- Compressed graceful path (emulated, never missing) ----
  add("FMT.BC_DECOMPRESS", "format", 3, Op::Mul, emu);
}

void RdnaIsaFull::InstallAll(CapabilityRegistry& reg) const {
  for (auto& e : entries_) {
    reg.Set(e.pipe_op, Stage::Decode, e.caps.decode);
    reg.Set(e.pipe_op, Stage::Cfg, e.caps.cfg);
    reg.Set(e.pipe_op, Stage::Translate, e.caps.translate);
    reg.Set(e.pipe_op, Stage::Ir, e.caps.ir);
    reg.Set(e.pipe_op, Stage::Backend, e.caps.backend);
  }
}

double RdnaIsaFull::CoverageAt(Stage stage) const {
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

std::vector<std::pair<std::string, double>> RdnaIsaFull::CoverageByClass(
    Stage stage) const {
  std::map<std::string, std::pair<size_t, size_t>> acc;
  for (auto& e : entries_) {
    auto& a = acc[e.cls];
    a.second++;
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
      a.first++;
  }
  std::vector<std::pair<std::string, double>> out;
  for (auto& [k, v] : acc)
    out.emplace_back(k, v.second ? double(v.first) / double(v.second) : 1.0);
  return out;
}

const IsaEntry* RdnaIsaFull::FindByName(const std::string& name) const {
  for (auto& e : entries_) {
    if (e.name == name) return &e;
  }
  return nullptr;
}

}  // namespace kyty::shader
