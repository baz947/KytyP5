"""Differential vs upstream KytyPS5 (submodule at upstream/KytyPS5).

Design (README: fixes must benefit every game sharing a requirement):
  upstream evidence (file/marker)  ->  our semantic owner  ->  status

Works in two modes:
  - submodule present: live-grep upstream tree + report pin (commit sha).
  - submodule absent (no network): static mapping table from README
    references + verify OUR side only (files/strings/corpus exist).

Exit code is always 0: the report carries the verdict. CI with network
checks out submodules and gets the live comparison; offline machines get
the static mapping verified against our tree.
"""
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
UPSTREAM = os.path.join(ROOT, "upstream", "KyTyPS5")

# Static mapping: (upstream marker, upstream path hint, our owner, our proof)
MARKERS = [
    ("ShaderRecompiler", "src/graphics/shader/recompiler/ShaderRecompiler.cpp",
     "shader-pipeline", "include/kyty/ShaderCore.h"),
    ("GetImageResource", "src/graphics/shader/recompiler/",
     "resource-tracking", "src/ResourceTracking.cpp:not a valid runtime value"),
    ("MaterializeResources", "src/graphics/",
     "resource-materialization", "include/kyty/ResourceMaterialization.h:NeedFallback"),
    ("MIMG", "src/graphics/shader/recompiler/",
     "mimg-decoder", "src/DescriptorDecoder.cpp:MimgDecoder"),
    ("EXEC", "src/graphics/shader/recompiler/",
     "wave-model", "src/WaveModel.cpp:Wave64"),
    ("BDA", "src/",
     "guest-memory", "include/kyty/GuestMemory.h:ResolveBda"),
    ("VideoOut", "src/",
     "videoout", "src/VideoOut.cpp:VBlankCount"),
    ("SaveData", "src/",
     "savedata", "src/SaveData.cpp:Commit"),
    ("TLS", "src/",
     "tls-manager", "src/TlsManager.cpp:FsBaseForThread"),
    ("libkernel", "src/",
     "service-registry", "src/ServiceRegistry.cpp:CheckDependencies"),
]


def _read(path):
    with open(path, encoding="utf-8", errors="replace") as f:
        return f.read()


def submodule_present():
    return os.path.isdir(os.path.join(UPSTREAM, ".git")) or os.path.isfile(
        os.path.join(UPSTREAM, "CMakeLists.txt"))


def submodule_pin():
    try:
        out = subprocess.run(
            ["git", "rev-parse", "HEAD"], cwd=UPSTREAM, capture_output=True,
            text=True, timeout=20)
        sha = out.stdout.strip()
        return sha if re.fullmatch(r"[0-9a-f]{40}", sha) else "unknown"
    except Exception:
        return "unknown"


def check_our_side():
    """Verify every mapping's proof exists in OUR tree. Returns rows."""
    rows = []
    for marker, upath, owner, proof in MARKERS:
        fpath, _, needle = proof.partition(":")
        full = os.path.join(ROOT, fpath)
        ok = os.path.isfile(full)
        found = False
        if ok and needle:
            try:
                found = needle in _read(full)
            except OSError:
                found = False
        elif ok:
            found = True
        rows.append({
            "marker": marker,
            "upstream_hint": upath,
            "owner": owner,
            "proof": proof,
            "our_file": ok,
            "our_marker": found,
            "status": "PASS" if (ok and found) else "MISSING",
        })
    return rows


def grep_upstream(marker, root=None, limit=200):
    """Live-grep submodule tree for marker. Returns hit count + sample files."""
    root = root or UPSTREAM
    hits, files = 0, []
    for dirpath, dirnames, filenames in os.walk(root):
        if ".git" in dirnames:
            dirnames.remove(".git")
        for fn in filenames:
            if not fn.endswith((".cpp", ".h", ".hpp", ".txt", ".md")):
                continue
            p = os.path.join(dirpath, fn)
            try:
                if os.path.getsize(p) > 2_000_000:
                    continue
                text = _read(p)
            except OSError:
                continue
            if marker in text:
                hits += 1
                if len(files) < 5:
                    files.append(os.path.relpath(p, root))
            if hits >= limit:
                return hits, files
    return hits, files


def build_report():
    present = submodule_present()
    rows = check_our_side()
    live = {}
    if present:
        for marker, _, _, _ in MARKERS:
            hits, files = grep_upstream(marker)
            live[marker] = {"hits": hits, "files": files}
    return {
        "mode": "live" if present else "static",
        "pin": submodule_pin() if present else None,
        "rows": rows,
        "live": live,
        "passed": sum(1 for r in rows if r["status"] == "PASS"),
        "missing": sum(1 for r in rows if r["status"] != "PASS"),
    }


def main():
    rep = build_report()
    print(f"diff_upstream mode={rep['mode']}"
          + (f" pin={rep['pin'][:12]}" if rep["pin"] else ""))
    print(f"our-side: {rep['passed']}/{len(rep['rows'])} PASS")
    for r in rep["rows"]:
        extra = ""
        if rep["mode"] == "live":
            lv = rep["live"].get(r["marker"], {})
            extra = f" upstream_hits={lv.get('hits', 0)}"
        print(f"  [{r['status']}] {r['marker']} -> {r['owner']}{extra}")
    if rep["mode"] == "static":
        print("submodule absent: run")
        print("  git submodule update --init --depth 1 upstream/KyTyPS5")
        print("for the live comparison (needs network).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
