import os, subprocess, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "python"))

ROOT = os.path.join(os.path.dirname(__file__), "..", "..")
PATCH_DIR = os.path.join(ROOT, "patches", "upstream")
UPSTREAM = os.path.join(ROOT, "upstream", "KyTyPS5")

def _present():
    return os.path.isdir(os.path.join(UPSTREAM, "src"))

def test_manifest_lists_existing_patches():
    mf = os.path.join(PATCH_DIR, "MANIFEST")
    assert os.path.isfile(mf)
    rows = [l.strip() for l in open(mf, encoding="utf-8")
            if l.strip() and not l.startswith("#")]
    assert rows, "no patches declared"
    for r in rows:
        parts = [p.strip() for p in r.split("|")]
        assert len(parts) == 5, f"bad manifest row: {r}"
        assert parts[0].endswith(".patch")
        assert os.path.isfile(os.path.join(PATCH_DIR, parts[0])), parts[0]
        assert parts[3] in ("diagnostic-only", "behavioral"), parts[3]

def test_patches_apply_clean():
    if not _present():
        return  # offline: covered by CI live step instead
    patches = sorted(f for f in os.listdir(PATCH_DIR) if f.endswith(".patch"))
    assert patches
    for p in patches:
        r = subprocess.run(
            ["git", "-C", UPSTREAM, "apply", "--check",
             os.path.join("..", "..", "patches", "upstream", p)],
            capture_output=True, text=True, timeout=60)
        assert r.returncode == 0, f"{p}: {r.stderr.strip()}"
