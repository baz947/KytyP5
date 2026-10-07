import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "python"))
from kyty.backend_bridge import lower, file_failures, SUBSYSTEM_FOR
from kyty.corpus_runner import BUILTIN

def test_lowering():
    kind, passes, mode = lower(True, False, False)
    assert kind == "Wave64Split" and len(passes) == 2 and mode == "w32x2"
    kind2, _, mode2 = lower(True, False, True)
    assert kind2 == "None" and mode2 != mode  # keys differ
    kind3, _, _ = lower(False, True, True)
    assert kind3 == "MimgCompare"

def test_bridge():
    assert SUBSYSTEM_FOR["wave64"] == "wave_model"
    assert SUBSYSTEM_FOR["bda"] == "guest_memory"
    db = {("PPSA00001", "01.00"): []}
    res = [("bda.resolve_ok", "fail", "mismatch", "addr=ok")]
    n = file_failures(db, "PPSA00001", "01.00", res, BUILTIN)
    assert n == 1 and db[("PPSA00001", "01.00")][0][0] == "guest_memory"
    assert file_failures(db, "GHOST", "00.00", res, BUILTIN) is None
    ev_titles = [k for k, v in db.items() if v]
    ev_cases = [c for c in BUILTIN if SUBSYSTEM_FOR.get(c[0]) == "guest_memory"]
    assert ev_titles and ev_cases
