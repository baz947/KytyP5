from enum import IntEnum
from .compat import RuntimeError, Result

class Format(IntEnum):
    Unknown = 0
    RGBA8 = 1
    SRGB8 = 2
    F16x4 = 3
    D32 = 4
    R32 = 5
    BC1 = 6

class FormatResolution(IntEnum):
    Native = 0
    CompatibleView = 1
    Reinterpreted = 2
    Emulated = 3
    Unsupported = 4

class ResolutionState(IntEnum):
    Static = 0
    Derived = 1
    Dynamic = 2
    Runtime = 3
    Unknown = 4
    Invalid = 5

class MaterializeResult(IntEnum):
    Success = 0
    NeedFallback = 1
    NeedShaderRewrite = 2
    NeedViewReinterpretation = 3
    Unsupported = 4
    InvalidDescriptor = 5
    InvalidMemory = 6

class Uniformity(IntEnum):
    Uniform = 0
    Divergent = 1

class WaveBackend(IntEnum):
    Native = 0
    Split = 1
    Software = 2

class GpuCapability(IntEnum):
    Native = 0
    Lowered = 1
    Emulated = 2
    Fallback = 3
    Unavailable = 4

def resolve_format(f, compressed_host=False):
    if f == Format.Unknown: return FormatResolution.Unsupported
    if f == Format.BC1:
        return FormatResolution.Native if compressed_host else FormatResolution.Emulated
    return FormatResolution.Native

def make_image(addr, fmt, width, mips=1, layers=1, mem_version=1):
    if not addr or fmt == Format.Unknown or not width:
        return Result.fail(RuntimeError.InvalidResource, "bad image")
    if mips == 0 or layers == 0:
        return Result.fail(RuntimeError.InvalidResource, "range")
    return Result.ok({"addr": addr, "fmt": fmt, "w": width, "mips": mips,
                       "layers": layers, "mem": mem_version})

def materialize(img, host_res):
    if not img or not img.get("addr"):
        return (MaterializeResult.InvalidDescriptor, "null")
    if host_res in (FormatResolution.Native, FormatResolution.CompatibleView):
        return (MaterializeResult.Success, "")
    if host_res == FormatResolution.Reinterpreted:
        return (MaterializeResult.NeedViewReinterpretation, "reinterpret")
    if host_res == FormatResolution.Emulated:
        return (MaterializeResult.NeedFallback, "fallback")
    if img.get("fmt") == Format.BC1:
        return (MaterializeResult.NeedShaderRewrite, "decompress")
    return (MaterializeResult.Unsupported, "no path")

def track_runtime(bda, valid):
    if not valid:
        return Result.fail(RuntimeError.InvalidResource,
                           "not a valid runtime value")
    if not bda:
        return Result.fail(RuntimeError.InvalidResource, "null BDA")
    return Result.ok({"addr": bda, "res": ResolutionState.Runtime})

def select_wave_backend(wave64, host64, allow_split):
    if not wave64: return Result.ok(WaveBackend.Native)
    if host64: return Result.ok(WaveBackend.Native)
    if allow_split: return Result.ok(WaveBackend.Split)
    return Result.fail(RuntimeError.UnsupportedFeature, "wave64 lowering")

def validate_barrier(exec_mask, wave=32):
    full = (1 << wave) - 1 if wave == 32 else (1 << 64) - 1
    active = exec_mask & (full & 0xFFFFFFFFFFFFFFFF)
    if not active:
        return Result.fail(RuntimeError.InvalidArgument, "empty EXEC")
    if active != (full & 0xFFFFFFFFFFFFFFFF):
        return Result.fail(RuntimeError.ShaderFailure, "divergent barrier")
    return Result.ok()

def resolve_req(req, feats):
    table = {
        "descriptors": "Native" if feats.get("desc_arrays") else "Emulated",
        "bda": "Native" if feats.get("bda") else "Emulated",
        "ray": "Lowered" if feats.get("ray") else "Unavailable",
        "wave64": "Native" if feats.get("sub64") else "Lowered",
        "imgatom": "Native" if feats.get("imgatom") else "Fallback",
    }
    return table.get(req, "Unavailable")

class GpuTimeline:
    def __init__(self):
        self._next = 1; self.live = {}; self.g2h = {}
    def submit(self, guest, res):
        if not guest:
            return Result.fail(RuntimeError.InvalidArgument, "guest")
        h = self._next; self._next += 1
        self.live[h] = res; self.g2h[guest] = h
        return Result.ok(h)
    def complete(self, h):
        if h not in self.live:
            return Result.fail(RuntimeError.InvalidArgument, "submit")
        del self.live[h]
        return Result.ok()
    def can_destroy(self, res):
        return res not in self.live.values()

def observe_hang(iters, cap=1_000_000):
    if iters < cap:
        return Result.fail(RuntimeError.InvalidArgument, "below cap")
    return Result.ok({"iterations": iters})
