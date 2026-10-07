from enum import IntEnum
from .compat import RuntimeError, Result

class ObjectType(IntEnum):
    Thread = 0
    Event = 1
    Mutex = 2
    Semaphore = 3
    File = 4
    Socket = 5
    GpuResource = 6
    VideoOut = 7

class HandleManager:
    def __init__(self):
        self._next = 1
        self._table = {}
    def create(self, typ):
        if self._next == 0xFFFFFFFF:
            return Result.fail(RuntimeError.HostMemoryFailure, "exhausted")
        h = self._next; self._next += 1
        if h == 0: h = self._next; self._next += 1
        self._table[h] = {"type": typ, "alive": True}
        return Result.ok(h)
    def lookup(self, h):
        if h == 0: return Result.fail(RuntimeError.InvalidArgument, "handle 0")
        e = self._table.get(h)
        if not e: return Result.fail(RuntimeError.InvalidArgument, "stale handle")
        return Result.ok(e)
    def destroy(self, h):
        if h not in self._table:
            return Result.fail(RuntimeError.InvalidArgument, "stale handle")
        del self._table[h]
        return Result.ok()
    def alive(self): return len(self._table)
