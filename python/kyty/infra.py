from .compat import RuntimeError, Result

class CompatDatabase:
    def __init__(self): self.t = {}; self.scores = {}
    def upsert(self, tid, ver, status="ingame", req=None, issues=()):
        if not tid:
            return Result.fail(RuntimeError.InvalidArgument, "title")
        self.t[(tid, ver)] = {"status": status, "req": req or {},
                               "issues": list(issues)}
        return Result.ok()
    def lookup(self, tid, ver):
        if (tid, ver) not in self.t:
            return Result.fail(RuntimeError.InvalidArgument, "unknown")
        return Result.ok(self.t[(tid, ver)])
    def affected_by(self, subsys):
        return [t for t in self.t.values()
                if any(i[0] == subsys for i in t["issues"])]
    def set_score(self, tid, ver, dims):
        if (tid, ver) not in self.t:
            return Result.fail(RuntimeError.InvalidArgument, "unknown")
        self.scores[(tid, ver)] = dict(dims)
        return Result.ok()
    def score(self, tid, ver):
        if (tid, ver) not in self.scores:
            return Result.fail(RuntimeError.InvalidArgument, "no score")
        d = self.scores[(tid, ver)]
        return Result.ok(sum(d.values()) // len(d))

class TraceCollector:
    def __init__(self): self.ev = []
    def emit(self, frame, submit, subsys, msg):
        self.ev.append((frame, submit, subsys, msg))
    def slice(self, frame, submit):
        return [e for e in self.ev if e[0] == frame and e[1] == submit]

def build_report(fault, ctx):
    """fault: dict(kind,addr,size,access,thread,pc,queue,submit,res,shader,retries,frame).
    ctx: dict(module,abi,tls,res_fp,host,recovery). Pure: all fields derive."""
    return {
        "pc": fault["pc"], "frame": fault.get("frame", 0),
        "module": ctx.get("module", ""), "abi": ctx.get("abi", "sysv-amd64"),
        "thread_tls": f"thread={fault['thread']} tls={ctx.get('tls', 0):#x}",
        "mem": f"{fault['addr']:#x}+{fault['size']:#x} acc={fault['access']}",
        "resource": ctx.get("res_fp") or f"res={fault['res']}",
        "shader": fault["shader"],
        "queue": f"q={fault['queue']} submit={fault['submit']} retry={fault['retries']}",
        "host": ctx.get("host", ""), "recovery": ctx.get("recovery", ""),
    }

class Chaos:
    def __init__(self, faults=()): self.f = set(faults)
    def gpu_submit(self):
        if "device-lost" in self.f:
            return Result.fail(RuntimeError.HostGpuFailure, "device lost")
        return Result.ok()
    def mem(self):
        if "mem-fault" in self.f:
            return Result.fail(RuntimeError.InvalidGuestMemory, "fault")
        return Result.ok()
    def bind(self):
        if "desc-miss" in self.f:
            return Result.fail(RuntimeError.InvalidResource, "miss")
        return Result.ok()

def xorshift(seed):
    s = seed or 0x9E3779B97F4A7C15
    s ^= (s << 13) & 0xFFFFFFFFFFFFFFFF
    s ^= s >> 7
    s ^= (s << 17) & 0xFFFFFFFFFFFFFFFF
    return s or 0x9E3779B97F4A7C15

def fuzz_bytes(seed, n):
    s = seed; out = bytearray()
    for _ in range(n):
        s = xorshift(s)
        out.append((s >> 33) & 0xFF)
    return bytes(out)
