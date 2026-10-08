import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "python"))
from kyty.p2_rdna import FULL, LruCache, PresentSim

def test_full_isa():
    assert len(FULL) >= 50
    for n in ("MIMG.SAMPLE_P", "MIMG.ATOMIC_CMPXCHG", "DS.ADD_U32",
              "FLAT.LOAD", "VOP.MAD_F32", "SOP1.S_MOV_B32",
              "VOP3P.V_PK_ADD_F16", "VINTRP.V_INTERP_P1_F32", "EXP.EXPORT"):
        assert n in FULL

def test_cache_present():
    c = LruCache(2)
    assert c.lookup("a") is None
    c.store("a", 1); c.store("b", 2)
    assert c.lookup("a") == 1
    c.store("c", 3)
    assert c.lookup("b") is None and c.lookup("c") == 3
    assert c.hits >= 2 and c.misses >= 2 and c.evicts == 1
    p = PresentSim()
    assert p.submit(101) and not p.submit(0)
    p.advance(20_000_000)
    assert p.present_due() == [101] and p.presented == 1
    m = PresentSim(mode="mailbox", depth=2)
    m.submit(1); m.submit(2); m.submit(3)
    assert m.dropped == 1
    m.advance(20_000_000)
    assert len(m.present_due()) == 1
    lr = PresentSim()
    lr.submit(7); lr.advance(20_000_000)
    lr.present_due()  # baseline
    lr.submit(8); lr.advance(100_000_000)
    lr.present_due()
    assert lr.stalls == 1
