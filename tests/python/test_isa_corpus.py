import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "python"))
from kyty.isa_table import ENTRIES, coverage_at, find
from kyty.corpus_runner import CorpusRunner, BUILTIN, load_manifest

def test_isa():
    assert len(ENTRIES) >= 10
    assert find("MIMG.SAMPLE") and find("SALU.S_BARRIER")
    assert coverage_at("decode") == 1.0
    assert coverage_at("backend") > 0.9

def test_corpus():
    r = CorpusRunner({})
    r.add_all()
    assert len(r.cases) == 12
    res = r.run(lambda inp: {"wave64:split": "backend=split"}.get(inp, inp) if False else _echo(inp))
    passed = sum(1 for _, s, _, _ in res if s == "pass")
    skipped = sum(1 for _, s, _, _ in res if s == "skipped")
    assert skipped == 1 and passed == 11
    # differential mismatch -> explicit fail, not crash
    r2 = CorpusRunner({})
    r2.add_all([BUILTIN[4]])
    res2 = r2.run(lambda inp: "mismatch")
    assert res2[0][1] == "fail"
    # manifests on disk parse and cover every builtin case
    base = os.path.join(os.path.dirname(__file__), "..", "corpus")
    disk = []
    for root, _, files in os.walk(base):
        if "manifest" in files:
            disk.extend(load_manifest(os.path.join(root, "manifest")))
    assert len(disk) >= 12
    for _, name, _, _, exp in BUILTIN:
        assert any(n == name and e == exp for _, n, _, _, e in disk), name

def _echo(inp):
    table = {
        "wave64:split": "backend=split", "desc:idx=0": "view=ok",
        "fmt=BC1": "fallback=emu", "fmt=D32": "native",
        "bda:valid": "addr=ok", "atomic:image": "ok", "exec=full": "ok",
        "flip:60fps": "vblank+1", "stage+commit": "read=ok",
        "2threads": "addr!=addr", "send/recv": "echo=ok",
        "ray:query": "lowered", "wave64:native": "backend=native",
        "desc:idx=divergent": "view=ok+divergent", "fmt=RGBA8": "native",
        "fmt=SRGB": "reinterpret", "fmt=D24S8": "compatible-view",
        "bda:invalid": "error=InvalidGuestMemory", "atomic:buf64": "ok-or-emulated",
        "exec=partial": "error=ShaderFailure",
    }
    return table.get(inp, inp)
