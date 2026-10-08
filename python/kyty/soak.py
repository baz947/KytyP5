"""Soak runner + durable CompatDB mirror (json)."""
import json
import os


class CompatDatabase:
    def __init__(self): self.t = {}
    def upsert(self, tid, ver, entry):
        self.t[(tid, ver)] = entry
    def save(self, path):
        with open(path, "w", encoding="utf-8") as f:
            json.dump({"v": 1, "titles": [
                {"id": tid, "ver": ver, **e} for (tid, ver), e in self.t.items()
            ]}, f)
    def load(self, path):
        try:
            with open(path, encoding="utf-8") as f:
                d = json.load(f)
            if d.get("v") != 1 or "titles" not in d:
                return False
            self.t = {(t["id"], t["ver"]): t for t in d["titles"]}
            return True
        except (OSError, ValueError, KeyError, TypeError):
            return False


def soak(cases, feats, chaos=(), harness=None):
    """cases: BUILTIN-style tuples. chaos: fault names armed.
    harness(inp)->expected|raises. Returns (results, filed)."""
    hook_for = {"bda": "mem", "image_formats": "bind",
                "depth_stencil": "bind", "dynamic_descriptors": "bind"}
    results, filed = [], 0
    for feat, name, req, inp, exp in cases:
        if req == "RayTracing" and not feats.get("ray"):
            results.append((name, "skipped", "", ""))
            continue
        hook = hook_for.get(feat, "gpu")
        armed = {"gpu": "device-lost", "mem": "mem-fault",
                 "bind": "desc-miss"}[hook]
        if armed in chaos:
            results.append((name, "fail", f"ERROR:chaos:{armed}", exp))
            filed += 1
            continue
        try:
            act = harness(inp)
            results.append((name, "pass" if act == exp else "fail", act, exp))
            if act != exp:
                filed += 1
        except Exception as e:
            results.append((name, "fail", f"ERROR:{e}", exp))
            filed += 1
    return results, filed
