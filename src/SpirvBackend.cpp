#include "kyty/SpirvBackend.h"
#include <map>

namespace kyty::shader::spirv {

namespace {

void Push(std::vector<uint32_t>& w, uint32_t op, uint32_t len) {
  w.push_back(op | (len << 16));
}

void PushStr(std::vector<uint32_t>& w, const std::string& s) {
  std::string z = s;
  z.push_back('\0');
  while (z.size() % 4) z.push_back('\0');
  for (size_t i = 0; i < z.size(); i += 4) {
    uint32_t word = uint32_t(uint8_t(z[i])) |
                    (uint32_t(uint8_t(z[i + 1])) << 8) |
                    (uint32_t(uint8_t(z[i + 2])) << 16) |
                    (uint32_t(uint8_t(z[i + 3])) << 24);
    w.push_back(word);
  }
}

const char* IrName(IrOp op) {
  switch (op) {
    case IrOp::Copy: return "Copy";
    case IrOp::Add: return "Add";
    case IrOp::Mul: return "Mul";
    case IrOp::ImageSample: return "ImageSample";
    case IrOp::ReadLane: return "ReadLane";
    case IrOp::WriteLane: return "WriteLane";
    case IrOp::Broadcast: return "Broadcast";
    case IrOp::Shuffle: return "Shuffle";
    case IrOp::Ballot: return "Ballot";
    case IrOp::ActiveMask: return "ActiveMask";
    case IrOp::Barrier: return "Barrier";
    case IrOp::Atomic: return "Atomic";
    case IrOp::Phi: return "Phi";
    case IrOp::Dead: return "Dead";
    default: return "Unknown";
  }
}

}  // namespace

Result<SpirvModule> SpirvEmitter::Emit(const IrModule& ir,
                                       const std::string& stage) {
  // Collect live instructions; capability-gated ops fail explicitly.
  std::vector<const IrInst*> live;
  for (auto& ii : ir.insts) {
    if (ii.op == IrOp::Dead) continue;
    if (ii.op != IrOp::Copy && ii.op != IrOp::Add && ii.op != IrOp::Mul) {
      return Result<SpirvModule>::Fail(
          RuntimeError::UnsupportedFeature,
          std::string("SPIR-V emission needs lowering: ") + IrName(ii.op));
    }
    if (ii.args.size() < 2) {
      return Result<SpirvModule>::Fail(RuntimeError::InvalidArgument,
                                       "short ALU args");
    }
    live.push_back(&ii);
  }

  // Id plan: 1=void, 2=u32, 3=fnTy, then one function param per distinct
  // source value (flat operand space: every toy source becomes a param),
  // then one result id per instruction. Bound = next free.
  std::map<uint32_t, uint32_t> param_of;
  for (auto* ii : live) {
    for (auto a : ii->args) {
      if (!param_of.count(a)) param_of[a] = 0;
    }
  }
  uint32_t next = 4;
  for (auto& [_, id] : param_of) id = next++;
  std::map<uint32_t, uint32_t> result_of;  // IrInst id -> spirv id
  for (auto* ii : live) result_of[ii->id] = next++;

  std::vector<uint32_t> w;
  w.reserve(32 + live.size() * 5);
  // Header (bound patched at end).
  w.push_back(kMagic);
  w.push_back(kVersion13);
  w.push_back(0);  // generator: unknown
  w.push_back(0);  // bound placeholder
  w.push_back(0);  // schema
  // OpCapability Shader(1).
  Push(w, kOpCapability, 2);
  w.push_back(1);
  // OpMemoryModel Logical(0) GLSL450(1).
  Push(w, kOpMemoryModel, 3);
  w.push_back(0);
  w.push_back(1);
  // Types.
  Push(w, kOpTypeVoid, 2);
  w.push_back(1);
  Push(w, kOpTypeInt, 4);
  w.push_back(2);
  w.push_back(32);
  w.push_back(0);
  {  // OpTypeFunction void(u32 x N).
    uint32_t n = uint32_t(param_of.size());
    Push(w, kOpTypeFunction, 3 + n);
    w.push_back(3);
    w.push_back(1);
    for (auto& [_, id] : param_of) {
      (void)id;
      w.push_back(2);
    }
  }
  uint32_t fn_id = next++;
  // OpEntryPoint GLCompute(5) + OpExecutionMode LocalSize(17,1,1,1).
  std::string entry = (stage.empty() ? "cs" : stage) + "_main";
  size_t str_words = (entry.size() + 1 + 3) / 4;
  Push(w, kOpEntryPoint, uint32_t(3 + str_words));
  w.push_back(5);
  w.push_back(fn_id);
  PushStr(w, entry);
  Push(w, kOpExecutionMode, 6);
  w.push_back(fn_id);
  w.push_back(17);
  w.push_back(1);
  w.push_back(1);
  w.push_back(1);
  // OpFunction void ... OpLabel, params, body, OpReturn, OpFunctionEnd.
  Push(w, kOpFunction, 5);
  w.push_back(1);
  w.push_back(fn_id);
  w.push_back(0);
  w.push_back(3);
  for (auto& [_, id] : param_of) {
    Push(w, kOpFunctionParameter, 3);
    w.push_back(2);
    w.push_back(id);
  }
  uint32_t label = next++;
  Push(w, kOpLabel, 2);
  w.push_back(label);
  auto operand = [&](uint32_t toy) -> uint32_t {
    auto pit = param_of.find(toy);
    return pit != param_of.end() ? pit->second : 0;
  };
  for (auto* ii : live) {
    uint32_t dst = result_of[ii->id];
    uint32_t a = operand(ii->args[0]);
    uint32_t b = operand(ii->args[1]);
    if (!a || !b) {
      return Result<SpirvModule>::Fail(RuntimeError::InvalidArgument,
                                       "operand has no SSA root");
    }
    if (ii->op == IrOp::Add) {
      Push(w, kOpIAdd, 5);
    } else if (ii->op == IrOp::Mul) {
      Push(w, kOpIMul, 5);
    } else {
      Push(w, kOpCopyObject, 4);
      w.push_back(2);
      w.push_back(dst);
      w.push_back(a);
      continue;
    }
    w.push_back(2);
    w.push_back(dst);
    w.push_back(a);
    w.push_back(b);
  }
  Push(w, kOpReturn, 1);
  Push(w, kOpFunctionEnd, 1);
  w[3] = next;  // bound

  SpirvModule m;
  m.words = std::move(w);
  m.bound = next;
  return Result<SpirvModule>::Ok(std::move(m));
}

Result<void> SpirvValidator::Validate(const uint32_t* words, size_t count) {
  auto fail = [](const char* d) {
    return Result<void>::Fail(RuntimeError::InvalidResource, d);
  };
  if (!words || count < 5) return fail("too short for header");
  if (words[0] != kMagic) return fail("bad magic");
  if (words[1] == 0) return fail("zero version");
  uint32_t bound = words[3];
  if (bound == 0) return fail("zero bound");
  if (words[4] != 0) return fail("nonzero schema");
  bool entry = false, in_fn = false, label = false;
  size_t i = 5;
  while (i < count) {
    uint32_t head = words[i];
    uint32_t len = head >> 16;
    uint32_t op = head & 0xFFFF;
    if (len < 1 || i + len > count) return fail("bad instruction length");
    auto id_ok = [&](uint32_t id) { return id != 0 && id < bound; };
    switch (op) {
      case kOpEntryPoint:
        if (len < 3) return fail("short entry point");
        if (!id_ok(words[i + 2])) return fail("entry id OOB");
        entry = true;
        break;
      case kOpFunction:
        if (len != 5) return fail("bad function header");
        if (in_fn) return fail("nested function");
        if (!id_ok(words[i + 2])) return fail("function id OOB");
        in_fn = true;
        label = false;
        break;
      case kOpLabel:
        if (!in_fn) return fail("label outside function");
        if (label) return fail("second label unsupported");
        if (!id_ok(words[i + 1])) return fail("label id OOB");
        label = true;
        break;
      case kOpReturn:
        if (!in_fn || !label) return fail("return outside body");
        break;
      case kOpFunctionEnd:
        if (!in_fn) return fail("end without function");
        in_fn = false;
        break;
      case kOpIAdd:
      case kOpIMul:
        if (len != 5) return fail("bad alu length");
        if (!in_fn || !label) return fail("alu outside body");
        for (uint32_t k = 1; k <= 4; ++k) {
          if (k == 1) continue;  // result type checked below
          if (!id_ok(words[i + k])) return fail("alu id OOB");
        }
        if (!id_ok(words[i + 1])) return fail("alu type OOB");
        break;
      case kOpCopyObject:
        if (len != 4) return fail("bad copy length");
        if (!in_fn || !label) return fail("copy outside body");
        if (!id_ok(words[i + 1]) || !id_ok(words[i + 2]) ||
            !id_ok(words[i + 3]))
          return fail("copy id OOB");
        break;
      default:
        break;  // types/caps/modes: length already bounds-checked
    }
    i += len;
  }
  if (in_fn) return fail("unterminated function");
  if (!entry) return fail("missing entry point");
  return Result<void>::Ok();
}

}  // namespace kyty::shader::spirv
