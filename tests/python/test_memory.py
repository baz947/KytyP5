import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "python"))
from kyty import GuestMemory, PageState, Access, RuntimeError

def test_roundtrip_and_cross_page():
    m = GuestMemory()
    assert m.map(0x10000, 0x4000*2, PageState.ReadWrite).is_ok()
    assert m.write(0x10000, b"kyty\x00").is_ok()
    r = m.read(0x10000, 5)
    assert r.is_ok() and r.value == b"kyty\x00"
    assert m.write(0x10000+0x4000-2, b"ABCD").is_ok()
    assert m.read(0x10000+0x4000-2, 4).value == b"ABCD"

def test_unmapped_is_structured_error():
    m = GuestMemory()
    r = m.read(0x90000, 4)
    assert not r.is_ok() and r.error == RuntimeError.InvalidGuestMemory

def test_protect_enforced():
    m = GuestMemory()
    m.map(0x10000, 0x4000, PageState.ReadWrite)
    m.protect(0x10000, 0x4000, int(Access.Read))
    assert not m.write(0x10000, b"xxxxx").is_ok()
    # range guards mirror C++ CheckRange (no silent Ok, no huge range())
    assert not m.protect(0x10000, 0, int(Access.Read)).is_ok()
    assert not m.synchronize(0x10000, 0).is_ok()
    assert not m.protect(1 << 48, 0x4000, int(Access.Read)).is_ok()

def test_bda_bounded_retry():
    m = GuestMemory()
    r = m.resolve_bda(0x90000, 8, lambda i: False)
    assert not r.is_ok()
    calls = {"n": 0}
    m2 = GuestMemory()
    def resolver(i):
        calls["n"] += 1
        if calls["n"] == 2:
            m2.map(0x20000, 0x4000, PageState.ReadWrite)
        return True
    assert m2.resolve_bda(0x20000, 8, resolver).is_ok()

def test_versioning():
    m = GuestMemory()
    m.map(0x20000, 0x4000, PageState.ReadWrite)
    v0, g0 = m.cpu_version, m.gpu_version
    m.write(0x20000, b"x"); m.synchronize(0x20000, 8)
    assert m.cpu_version > v0 and m.gpu_version > g0
