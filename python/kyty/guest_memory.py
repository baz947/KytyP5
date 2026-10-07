from dataclasses import dataclass
from enum import IntEnum
from .compat import RuntimeError, Result

PAGE_SIZE = 0x4000
ADDR_SPACE = 1 << 48

class PageState(IntEnum):
    Unmapped = 0
    Reserved = 1
    Committed = 2
    ReadOnly = 3
    ReadWrite = 4
    Executable = 5
    GpuVisible = 6
    Faulting = 7
    Evicted = 8

class Access(IntEnum):
    Read = 1
    Write = 2
    Execute = 4

BDA_MAX_RETRIES = 8

@dataclass
class PageInfo:
    state: PageState = PageState.Unmapped
    perms: int = 0
    version: int = 0

class GuestMemory:
    def __init__(self):
        self.backing = bytearray()
        self.base = 0
        self.backed_size = 0
        self.pages: dict[int, PageInfo] = {}
        self.cpu_version = 1
        self.gpu_version = 1
        self.mapping_version = 1

    @staticmethod
    def _page_base(a): return a & ~(PAGE_SIZE - 1)

    def _check(self, addr, size):
        if size == 0:
            return Result.fail(RuntimeError.InvalidArgument, "size==0")
        if addr >= ADDR_SPACE or size >= ADDR_SPACE or addr + size > ADDR_SPACE:
            return Result.fail(RuntimeError.InvalidArgument, "out of range")
        return Result.ok()

    def map(self, addr, size, state):
        c = self._check(addr, size)
        if not c.is_ok(): return c
        if addr % PAGE_SIZE or size % PAGE_SIZE:
            return Result.fail(RuntimeError.InvalidArgument, "page-aligned required")
        if state == PageState.Unmapped:
            return Result.fail(RuntimeError.InvalidArgument, "Map Unmapped")
        if not self.backing:
            self.base, self.backed_size = addr, size
            self.backing = bytearray(size)
        else:
            end, cur_end = addr + size, self.base + self.backed_size
            if addr < self.base or end > cur_end:
                nb = min(addr, self.base); ne = max(end, cur_end)
                nbuf = bytearray(ne - nb)
                nbuf[self.base - nb:self.base - nb + len(self.backing)] = self.backing
                self.backing, self.base, self.backed_size = nbuf, nb, ne - nb
        perms = {"ReadOnly": 1, "ReadWrite": 3, "Executable": 5}.get(state.name, 3)
        for p in range(self._page_base(addr), addr + size, PAGE_SIZE):
            self.mapping_version += 1
            self.pages[p] = PageInfo(state, perms, self.mapping_version)
        self.mapping_version += 1
        return Result.ok()

    def unmap(self, addr, size):
        c = self._check(addr, size)
        if not c.is_ok(): return c
        for p in range(self._page_base(addr), addr + size, PAGE_SIZE):
            self.pages.pop(p, None)
        self.mapping_version += 1
        return Result.ok()

    def protect(self, addr, size, perms):
        for p in range(self._page_base(addr), addr + size, PAGE_SIZE):
            if p not in self.pages:
                return Result.fail(RuntimeError.InvalidGuestMemory, "unmapped page")
            self.pages[p].perms = perms
            self.mapping_version += 1
            self.pages[p].version = self.mapping_version
        return Result.ok()

    def synchronize(self, addr, size):
        self.gpu_version += 1
        return Result.ok()

    def translate(self, addr, size, access: Access):
        c = self._check(addr, size)
        if not c.is_ok(): return c
        for p in range(self._page_base(addr), addr + size, PAGE_SIZE):
            pi = self.pages.get(p)
            if pi is None:
                return Result.fail(RuntimeError.InvalidGuestMemory, "unmapped page")
            if not (pi.perms & int(access)):
                return Result.fail(RuntimeError.InvalidGuestMemory, "permission denied")
            if pi.state in (PageState.Evicted, PageState.Faulting):
                return Result.fail(RuntimeError.InvalidGuestMemory, "not resident")
        if not self.backing or addr < self.base or addr + size > self.base + self.backed_size:
            return Result.fail(RuntimeError.HostMemoryFailure, "no host mapping")
        return Result.ok({"host_offset": addr - self.base, "version": self.mapping_version})

    def read(self, addr, size):
        t = self.translate(addr, size, Access.Read)
        if not t.is_ok(): return t
        off = t.value["host_offset"]
        return Result.ok(bytes(self.backing[off:off + size]))

    def write(self, addr, data: bytes):
        t = self.translate(addr, len(data), Access.Write)
        if not t.is_ok(): return t
        off = t.value["host_offset"]
        self.backing[off:off + len(data)] = data
        self.cpu_version += 1
        return Result.ok()

    def resolve_bda(self, addr, size, resolve_once):
        for attempt in range(BDA_MAX_RETRIES):
            t = self.translate(addr, size, Access.Read)
            if t.is_ok(): return t
            if t.error != RuntimeError.InvalidGuestMemory: return t
            if not resolve_once(attempt):
                return Result.fail(RuntimeError.InvalidGuestMemory,
                    f"BDA persistent fault, no progress (attempt {attempt})")
        return Result.fail(RuntimeError.InvalidGuestMemory, "BDA bounded retries exhausted")
