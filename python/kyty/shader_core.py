from .compat import RuntimeError, Result

# Toy encoding: word = op<<16 | a0<<8 | a1
OP_NOP, OP_MOV, OP_ADD, OP_MUL = 0, 1, 2, 3
OP_MIMG, OP_RDLANE, OP_BARRIER, OP_ATOMIC = 0x10, 0x20, 0x30, 0x31
OP_BRANCH, OP_CBRANCH, OP_RET = 0x40, 0x41, 0x42

COVERED = "covered"
MISSING = "missing"

class Registry:
    def __init__(self):
        self.backend = {OP_MIMG: "lowered"}
    def get(self, op, stage):
        if op == 0xFFFF: return MISSING
        if stage == "backend": return self.backend.get(op, COVERED)
        return COVERED

def decode(words, reg):
    out = []
    for w in words:
        op = (w >> 16) & 0xFFFF
        if reg.get(op, "decode") == MISSING:
            return Result.fail(RuntimeError.UnsupportedFeature, "decode")
        out.append((op, (w >> 8) & 0xFF, w & 0xFF))
    return Result.ok(out)

def build_cfg(insts):
    n = len(insts)
    if not n:
        return Result.fail(RuntimeError.InvalidArgument, "empty")
    leaders = {0}
    for i, (op, a0, a1) in enumerate(insts):
        if op in (OP_BRANCH, OP_CBRANCH, OP_RET) and i + 1 < n:
            leaders.add(i + 1)
        if op in (OP_BRANCH, OP_CBRANCH) and a0 < n:
            leaders.add(a0)
    bounds = sorted(leaders)
    blocks = []
    for bi, b in enumerate(bounds):
        e = bounds[bi + 1] if bi + 1 < len(bounds) else n
        blocks.append({"id": bi, "begin": b, "end": e})
    # edges
    idx2b = {}
    for bl in blocks:
        for i in range(bl["begin"], bl["end"]): idx2b[i] = bl["id"]
    edges = 0
    for bl in blocks:
        op, a0, _ = insts[bl["end"] - 1]
        if op == OP_BRANCH: edges += 1
        elif op == OP_CBRANCH: edges += 2
        elif op == OP_RET: edges += 0
        else: edges += 1 if bl["end"] < n else 0
    return Result.ok({"blocks": blocks, "edges": edges})

def translate(insts, reg):
    ir = []
    for op, a0, a1 in insts:
        if op in (OP_BRANCH, OP_CBRANCH, OP_RET, OP_NOP): continue
        if reg.get(op, "translate") == MISSING:
            return Result.fail(RuntimeError.UnsupportedFeature, "translate")
        if op == OP_MOV and a0 == a1: continue  # folded (DCE)
        ir.append((op, a0, a1))
    return Result.ok(ir)

def compile_words(words, stage, reg=None):
    reg = reg or Registry()
    d = decode(words, reg)
    if not d.is_ok(): return d
    c = build_cfg(d.value)
    if not c.is_ok(): return c
    t = translate(d.value, reg)
    if not t.is_ok(): return t
    h = 1469598103934665603
    for w in words:
        h ^= w; h = (h * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    info = {"hash": h, "stage": stage,
            "images": sum(1 for o, _, _ in t.value if o == OP_MIMG),
            "atomics": any(o == OP_ATOMIC for o, _, _ in t.value)}
    return Result.ok({"ir": t.value, "cfg": c.value, "info": info,
                       "spirv": h ^ 0x9E3779B97F4A7C15})
