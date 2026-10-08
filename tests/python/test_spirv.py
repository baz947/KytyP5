import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "python"))
from kyty.spirv import emit, validate, MAGIC

def test_emit_valid():
    w = emit([("add", 1, 2), ("mul", 2, 3), ("copy", 4, 5)], "cs")
    assert w[0] == MAGIC and validate(w)
    assert emit([], "ps") and validate(emit([], "ps"))

def test_emit_gated():
    try:
        emit([("image", 0, 0)], "ps")
        assert False
    except ValueError:
        pass

def test_validate_rejects():
    w = emit([("add", 1, 2)], "cs")
    bad = list(w)
    bad[0] = 0xDEADBEEF
    assert not validate(bad)
    assert not validate(w[:6])
    zb = list(w)
    zb[3] = 0
    assert not validate(zb)
    assert not validate([])
    ov = list(w)
    ov[5] = (99 << 16) | 17
    assert not validate(ov)
