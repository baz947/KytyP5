from enum import IntEnum
from .compat import RuntimeError, Result

class ArgClass(IntEnum):
    Gpr = 0
    Sse = 1
    Memory = 2
    Void = 3

MAX_GPR, MAX_SSE = 6, 8

def decode(frame_gpr, frame_xmm, frame_stack, params, variadic=False, al=0):
    ng = sum(1 for p in params if p == ArgClass.Gpr)
    ns = sum(1 for p in params if p == ArgClass.Sse)
    if ng > MAX_GPR or ns > MAX_SSE:
        return Result.fail(RuntimeError.WrongABI, "exceeds regfile")
    if variadic and al > MAX_SSE:
        return Result.fail(RuntimeError.WrongABI, "bad AL")
    out = []
    gi, xi, si = 0, 0, 0
    for p in params:
        if p == ArgClass.Gpr:
            if gi < MAX_GPR: out.append(frame_gpr[gi]); gi += 1
            elif si < len(frame_stack): out.append(frame_stack[si]); si += 1
            else: return Result.fail(RuntimeError.WrongABI, "missing GPR")
        elif p == ArgClass.Sse:
            if xi < MAX_SSE: out.append(frame_xmm[xi]); xi += 1
            elif si < len(frame_stack): out.append(frame_stack[si]); si += 1
            else: return Result.fail(RuntimeError.WrongABI, "missing SSE")
        elif p == ArgClass.Memory:
            if si < len(frame_stack): out.append(frame_stack[si]); si += 1
            else: return Result.fail(RuntimeError.WrongABI, "missing mem")
        else:
            return Result.fail(RuntimeError.WrongABI, "void param")
    return Result.ok(out)

def validate_pointer(mem, addr, size, access):
    from .guest_memory import Access as A
    t = mem.translate(addr, size, A.Read if access == "r" else A.Write)
    if not t.is_ok(): return Result.fail(t.error, t.detail)
    return Result.ok()
