"""Minimal valid SPIR-V emitter + structural validator mirror."""
import struct

MAGIC, VER = 0x07230203, 0x00010300
CAP, MEMMODEL, ENTRY, EXEMODE = 17, 14, 15, 16
TVOID, TINT, TFN = 19, 21, 33
FN, FNPARAM, FNEND = 54, 55, 56
COPY, IADD, IMUL = 83, 124, 127
LABEL, RET = 248, 253


def _str_words(s):
    b = s.encode() + b"\x00"
    b += b"\x00" * (-len(b) % 4)
    return list(struct.unpack(f"<{len(b) // 4}I", b))


def emit(ir, stage="cs"):
    """ir: list of (op, a0, a1); op in {copy,add,mul}. Returns word list."""
    live = []
    for op, a0, a1 in ir:
        if op not in ("copy", "add", "mul"):
            raise ValueError(f"needs lowering: {op}")
        live.append((op, a0, a1))
    params, order = {}, []
    for _, a0, a1 in live:
        for a in (a0, a1):
            if a not in params:
                params[a] = None
                order.append(a)
    nxt = 4
    for a in order:
        params[a] = nxt
        nxt += 1
    dsts = [nxt + i for i in range(len(live))]
    nxt += len(live)
    fn = nxt
    nxt += 1
    w = [MAGIC, VER, 0, 0, 0,
         (2 << 16) | CAP, 1,
         (3 << 16) | MEMMODEL, 0, 1,
         (2 << 16) | TVOID, 1,
         (4 << 16) | TINT, 2, 32, 0]
    w += [(3 + len(order) << 16) | TFN, 3, 1] + [2] * len(order)
    sw = _str_words(f"{stage}_main")
    w += [(3 + len(sw) << 16) | ENTRY, 5, fn] + sw
    w += [(6 << 16) | EXEMODE, fn, 17, 1, 1, 1,
          (5 << 16) | FN, 1, fn, 0, 3]
    for a in order:
        w += [(3 << 16) | FNPARAM, 2, params[a]]
    lab = nxt
    nxt += 1
    w += [(2 << 16) | LABEL, lab]
    for (op, a0, a1), d in zip(live, dsts):
        if op == "add":
            w += [(5 << 16) | IADD, 2, d, params[a0], params[a1]]
        elif op == "mul":
            w += [(5 << 16) | IMUL, 2, d, params[a0], params[a1]]
        else:
            w += [(4 << 16) | COPY, 2, d, params[a0]]
    w += [(1 << 16) | RET, (1 << 16) | FNEND]
    w[3] = nxt
    return w


def validate(words):
    if not words or len(words) < 5:
        return False
    if words[0] != MAGIC or words[1] == 0 or words[3] == 0 or words[4] != 0:
        return False
    bound, entry, in_fn, label = words[3], False, False, False
    i = 5
    while i < len(words):
        ln, op = words[i] >> 16, words[i] & 0xFFFF
        if ln < 1 or i + ln > len(words):
            return False
        body = words[i + 1:i + ln]
        ok = lambda x: 0 < x < bound
        if op == ENTRY:
            if ln < 3 or not ok(body[1]):
                return False
            entry = True
        elif op == FN:
            if ln != 5 or in_fn or not ok(body[1]):
                return False
            in_fn, label = True, False
        elif op == LABEL:
            if not in_fn or label or not ok(body[0]):
                return False
            label = True
        elif op == RET:
            if not (in_fn and label):
                return False
        elif op == FNEND:
            if not in_fn:
                return False
            in_fn = False
        elif op in (IADD, IMUL):
            if ln != 5 or not (in_fn and label) or not all(map(ok, body)):
                return False
        elif op == COPY:
            if ln != 4 or not (in_fn and label) or not all(map(ok, body)):
                return False
        i += ln
    return entry and not in_fn
