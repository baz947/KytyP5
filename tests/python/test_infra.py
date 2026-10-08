import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "python"))
from kyty.infra import CompatDatabase, TraceCollector, Chaos, fuzz_bytes, xorshift, build_report
from kyty.elf_loader import parse_image
from kyty.shader_core import compile_words

def test_compatdb():
    db = CompatDatabase()
    assert db.upsert("PPSA00001", "01.00", issues=[("resource_tracking", "broken")]).is_ok()
    assert db.lookup("PPSA00001", "01.00").is_ok()
    assert not db.lookup("PPSA00001", "02.00").is_ok()
    assert len(db.affected_by("resource_tracking")) == 1
    assert not db.affected_by("audio")
    db.set_score("PPSA00001", "01.00", {"Boot": 100, "Menu": 90, "Gameplay": 40})
    assert db.score("PPSA00001", "01.00").value == 76

def test_trace():
    tc = TraceCollector()
    tc.emit(7, 42, "memory", "map")
    tc.emit(7, 42, "gpu", "submit")
    tc.emit(7, 43, "gpu", "other")
    assert len(tc.slice(7, 42)) == 2 and len(tc.slice(7, 43)) == 1

def test_fuzz_chaos():
    assert fuzz_bytes(1234, 64) == fuzz_bytes(1234, 64)
    assert fuzz_bytes(999, 64) != fuzz_bytes(1234, 64)
    # fuzz into real harnesses: structured fail only
    parse_image(fuzz_bytes(5, 128))
    compile_words([int.from_bytes(fuzz_bytes(11 + i, 4), "little") for i in range(4)], "ps")
    assert Chaos(()).gpu_submit().is_ok()
    assert not Chaos(("device-lost",)).gpu_submit().is_ok()
    assert not Chaos(("mem-fault",)).mem().is_ok()
    assert not Chaos(("desc-miss",)).bind().is_ok()

def test_failure_report():
    fault = {"kind": "bda", "addr": 0x90000, "size": 8, "access": 1,
             "thread": 100, "pc": 0x1234, "queue": 2, "submit": 42,
             "res": 7, "shader": 0xABCD, "retries": 3, "frame": 7}
    ctx = {"module": "app:RenderFrame", "tls": 0x1000000000,
           "host": "VkErrorDeviceLost", "recovery": "fallback:emulated-format"}
    r = build_report(fault, ctx)
    assert r["pc"] == 0x1234 and r["frame"] == 7
    assert r["module"] == "app:RenderFrame" and r["abi"] == "sysv-amd64"
    assert "thread=100" in r["thread_tls"]
    assert "0x90000" in r["mem"] and r["resource"] == "res=7"
    assert r["shader"] == 0xABCD and "submit=42" in r["queue"]
    assert r["host"] == "VkErrorDeviceLost"
