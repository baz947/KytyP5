import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "python"))
from kyty import RuntimeError
from kyty.shader_core import (Registry, compile_words, OP_MOV, OP_ADD,
                              OP_BRANCH, OP_CBRANCH, OP_RET)

def W(op, a0=0, a1=0): return (op << 16) | (a0 << 8) | a1

def test_pipeline():
    r = compile_words([W(1, 5, 5), W(2, 1, 2), W(3, 1, 2)], "ps")
    assert r.is_ok() and r.value["info"]["hash"]
    assert len(r.value["ir"]) == 2  # Mov folded
    c = compile_words([W(1, 1, 2), W(OP_CBRANCH, 3), W(2, 1, 1), W(OP_RET)], "vs")
    assert c.is_ok() and len(c.value["cfg"]["blocks"]) == 3
    assert c.value["cfg"]["edges"] == 3
    assert not compile_words([0xFFFF0000], "ps").is_ok()
    m = compile_words([W(0x10), W(0x31)], "ps")
    assert m.is_ok() and m.value["info"]["images"] == 1
    assert m.value["info"]["atomics"]
