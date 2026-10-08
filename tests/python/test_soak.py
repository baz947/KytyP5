import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "python"))
from kyty.soak import CompatDatabase, soak
from kyty.corpus_runner import BUILTIN

def test_db_durable():
    db = CompatDatabase()
    db.upsert("PPSA00001", "01.00", {"status": "ingame", "note": 'a"b\\c'})
    p = os.path.join(os.path.dirname(__file__), "soak_tmp.json")
    try:
        db.save(p)
        db2 = CompatDatabase()
        assert db2.load(p)
        assert db2.t[("PPSA00001", "01.00")]["note"] == 'a"b\\c'
        with open(p, "w") as f:
            f.write('{"v":1,"titles":[{bad')
        assert not db2.load(p)
        assert ("PPSA00001", "01.00") in db2.t  # untouched
        assert not db2.load(os.path.join(os.path.dirname(__file__), "nope.json"))
    finally:
        if os.path.exists(p):
            os.remove(p)

def _echo(inp):
    table = {"wave64:split": "backend=split", "desc:idx=0": "view=ok",
             "fmt=BC1": "fallback=emu", "fmt=D32": "native",
             "bda:valid": "addr=ok", "atomic:image": "ok", "exec=full": "ok",
             "flip:60fps": "vblank+1", "stage+commit": "read=ok",
             "2threads": "addr!=addr", "send/recv": "echo=ok"}
    return table[inp]

def test_soak_loop():
    calm, filed = soak(BUILTIN, {}, (), _echo)
    assert filed == 0 and all(s != "fail" for _, s, _, _ in calm)
    storm, filed2 = soak(BUILTIN, {}, ("device-lost",), _echo)
    assert filed2 > 0
    assert any(n == "vo.flip_paced" and s == "fail" for n, s, _, _ in storm)
    # bda rides the mem hook: calm under gpu storm
    assert any(n == "bda.resolve_ok" and s == "pass" for n, s, _, _ in storm)
    memstorm, _ = soak(BUILTIN, {}, ("mem-fault",), _echo)
    assert any(n == "bda.resolve_ok" and s == "fail" for n, s, _, _ in memstorm)
