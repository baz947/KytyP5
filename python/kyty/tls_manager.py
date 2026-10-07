from .compat import RuntimeError, Result

TLS_BASE = 1 << 40

def _align_up(v, a): return (v + a - 1) & ~(a - 1)

class TlsManager:
    def __init__(self):
        self.templates = {}
        self.threads = {}
        self._next = 0

    def register(self, owner, size, align=16):
        if not owner: return Result.fail(RuntimeError.InvalidArgument, "owner")
        if align == 0 or (align & (align - 1)):
            return Result.fail(RuntimeError.InvalidArgument, "align pow2")
        self.templates[owner] = (size, align)
        return Result.ok()

    def alloc_thread(self, tid):
        if not tid: return Result.fail(RuntimeError.InvalidArgument, "thread")
        if tid in self.threads:
            return Result.fail(RuntimeError.InvalidArgument, "exists")
        base = TLS_BASE + self._next
        offs = {}; cursor = 0
        for mod, (sz, al) in self.templates.items():
            cursor = _align_up(cursor, al)
            offs[mod] = cursor
            cursor += _align_up(sz, al)
        stride = _align_up(cursor + 64, 0x4000)
        self._next += stride
        self.threads[tid] = (base, offs)
        return Result.ok(base)

    def module_address(self, tid, mod, off):
        if tid not in self.threads:
            return Result.fail(RuntimeError.InvalidArgument, "no thread")
        base, offs = self.threads[tid]
        if mod not in offs:
            return Result.fail(RuntimeError.MissingDependency, "no template")
        sz, _ = self.templates[mod]
        if off >= sz:
            return Result.fail(RuntimeError.InvalidGuestMemory, "TLS OOB")
        return Result.ok(base + offs[mod] + off)

    def fs_base(self, tid):
        if tid not in self.threads:
            return Result.fail(RuntimeError.InvalidArgument, "no thread")
        return Result.ok(self.threads[tid][0])
