import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "python"))
from kyty import RuntimeError
from kyty.elf_loader import parse_image, make_test_image, MACHINE_X86_64
from kyty.guest_module import ModuleManager, ModuleState
from kyty.symbol_resolver import (SymbolResolver, SymbolKey, SymbolEntry,
                                  SymbolType, SymbolBinding)
from kyty.relocation_engine import (RelocType, RelocResult, apply_one)
from kyty.tls_manager import TlsManager
from kyty.abi_adapter import ArgClass, decode, validate_pointer
from kyty.guest_memory import GuestMemory, PageState
from kyty.fault_manager import FaultManager, FaultKind, FaultRecord, FaultAction

def test_elf():
    img = make_test_image(["libkernel.sprx"], "libtest.sprx", True)
    r = parse_image(img)
    assert r.is_ok() and r.value.machine == MACHINE_X86_64
    assert r.value.tls_present and r.value.needed
    assert not parse_image(b"\x7fELX\x00\x00").is_ok()

def test_lifecycle():
    mm = ModuleManager()
    a = mm.discover("libkernel.sprx").value
    assert mm.set_state(a, ModuleState.Loading).is_ok()
    assert mm.set_state(a, ModuleState.Mapped).is_ok()
    assert not mm.set_state(a, ModuleState.Running).is_ok()
    assert mm.set_state(a, ModuleState.Parsed).is_ok()

def test_deps():
    m2 = ModuleManager()
    lk = m2.discover("libkernel.sprx").value
    ap = m2.discover("app.sprx").value
    m2.add_dependency(ap, "libkernel.sprx")
    o = m2.resolve_order()
    assert o.is_ok() and o.value[0] == lk and o.value[-1] == ap
    m3 = ModuleManager()
    x = m3.discover("x.sprx").value
    m3.add_dependency(x, "ghost.sprx")
    assert not m3.resolve_order().is_ok()

def test_symbols():
    sr = SymbolResolver()
    sr.define(SymbolEntry(SymbolKey(1, "puts", SymbolType.Func, "1.0"), 1, 0x1000))
    sr.define(SymbolEntry(SymbolKey(1, "errno", SymbolType.Object, ""), 1, 0x2000))
    assert sr.resolve_function(1, "puts", "1.0").is_ok()
    assert not sr.resolve_function(1, "puts", "2.0").is_ok()
    w = sr.resolve(SymbolKey(1, "puts", SymbolType.Object, "1.0"))
    assert not w.is_ok() and w.error == RuntimeError.WrongABI
    assert not sr.resolve_function(1, "nope", "").is_ok()
    sr.define(SymbolEntry(SymbolKey(1, "tlsvar", SymbolType.Tls, ""), 1, 0))
    assert sr.resolve_tls(1, "tlsvar", "").is_ok()
    assert not sr.resolve_object(1, "tlsvar", "").is_ok()

def test_reloc():
    st, v, _ = apply_one(RelocType.Relative, 0x5000, 0x10, None, True, 0x4000, False)
    assert st == RelocResult.Applied and v == 0x4010
    st, _, _ = apply_one(RelocType.JumpSlot, 0x5008, 0, None, False, 0x4000, False)
    assert st == RelocResult.Deferred
    st, _, th = apply_one(RelocType.JumpSlot, 0x5008, 0, None, True, 0x4000, True)
    assert st == RelocResult.Stubbed and th.is_stub
    st, v, _ = apply_one(RelocType.JumpSlot, 0x5008, 0, 0x7777, True, 0x4000, False)
    assert st == RelocResult.Applied and v == 0x7777
    st, _, _ = apply_one(RelocType.TpOff64, 0x5010, 0, None, True, 0, False)
    assert st == RelocResult.Deferred
    st, _, _ = apply_one(RelocType.R64, 0, 0, 1, True, 0, False)
    assert st == RelocResult.Invalid

def test_tls():
    tm = TlsManager()
    tm.register(1, 64)
    tm.alloc_thread(100); tm.alloc_thread(101)
    t1 = tm.module_address(100, 1, 0); t2 = tm.module_address(101, 1, 0)
    assert t1.is_ok() and t2.is_ok() and t1.value != t2.value
    assert not tm.module_address(100, 1, 9999).is_ok()
    assert tm.fs_base(100).is_ok()

def test_abi_and_boundary():
    assert decode([1, 2, 3, 0, 0, 0], [0]*8, [],
                  [ArgClass.Gpr]*3).is_ok()
    assert not decode([1]*6, [0]*8, [],
                      [ArgClass.Gpr]*7).is_ok()
    m = GuestMemory()
    m.map(0x10000, 0x4000, PageState.ReadWrite)
    assert validate_pointer(m, 0x10000, 8, "r").is_ok()
    assert not validate_pointer(m, 0x90000, 8, "r").is_ok()
    fm = FaultManager()
    try:
        raise RuntimeError("boom")
    except Exception:
        fm.record(FaultRecord(kind=FaultKind.CpuPageFault, thread=100,
                              action=FaultAction.ControlledFailure))
    assert len(fm.records) == 1
