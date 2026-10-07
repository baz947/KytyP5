from .compat import RuntimeError, Result

BUILTIN = [
    ("wave64", "wave64.split_basic", "Wave64", "wave64:split", "backend=split"),
    ("dynamic_descriptors", "desc.bindless_0", "DynamicDescriptors", "desc:idx=0", "view=ok"),
    ("image_formats", "fmt.bc1_emu", "None", "fmt=BC1", "fallback=emu"),
    ("depth_stencil", "ds.d32", "None", "fmt=D32", "native"),
    ("bda", "bda.resolve_ok", "Bda", "bda:valid", "addr=ok"),
    ("atomics", "atom.img_add", "ImageAtomics", "atomic:image", "ok"),
    ("barriers", "barrier.full", "None", "exec=full", "ok"),
    ("videoout", "vo.flip_paced", "None", "flip:60fps", "vblank+1"),
    ("savedata", "save.commit", "None", "stage+commit", "read=ok"),
    ("tls", "tls.isolated", "None", "2threads", "addr!=addr"),
    ("network", "net.loopback", "None", "send/recv", "echo=ok"),
    ("ray_tracing", "rt.query", "RayTracing", "ray:query", "lowered"),
]

def _met(req, feats):
    if req in ("None", ""): return True
    if req == "RayTracing": return bool(feats.get("ray"))
    return True  # emulated/fallback still run via lowering

class CorpusRunner:
    def __init__(self, feats=None):
        self.feats = feats or {}
        self.cases = []
    def add_all(self, cases=None):
        self.cases.extend(cases or BUILTIN)
    def run(self, harness):
        out = []
        for feat, name, req, inp, exp in self.cases:
            if not _met(req, self.feats):
                out.append((name, "skipped", "", ""))
                continue
            act = harness(inp)
            out.append((name, "pass" if act == exp else "fail", act, exp))
        return out

def load_manifest(path):
    cases = []
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"): continue
            name, req, inp, exp = line.split("|")
            cases.append(("manifest", name, req, inp, exp))
    return cases
