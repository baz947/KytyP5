import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "python"))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "tools"))
import diff_upstream as du

def test_mapping_table_sane():
    assert len(du.MARKERS) >= 8
    for marker, upath, owner, proof in du.MARKERS:
        assert marker and upath and owner and proof
        assert "/" in proof or proof.endswith((".h", ".cpp"))

def test_our_side_all_pass():
    rows = du.check_our_side()
    missing = [r for r in rows if r["status"] != "PASS"]
    assert not missing, f"missing proofs: {missing}"

def test_report_structure():
    rep = du.build_report()
    assert rep["mode"] in ("live", "static")
    assert rep["passed"] + rep["missing"] == len(du.MARKERS)
    if rep["mode"] == "static":
        assert rep["pin"] is None
        assert not du.submodule_present()
    else:
        assert rep["pin"]  # live mode must report the pin
        for marker in ("ShaderRecompiler", "MIMG"):
            assert marker in rep["live"]
