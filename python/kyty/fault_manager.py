import time
from dataclasses import dataclass, field
from enum import IntEnum
from collections import Counter
from .compat import RuntimeError, Result

class FaultKind(IntEnum):
    CpuPageFault = 0
    GpuPageFault = 1
    BdaFault = 2
    ResourceFault = 3
    ShaderHang = 4
    HostGpuFailure = 5
    Timeout = 6
    Deadlock = 7

class FaultAction(IntEnum):
    Resolved = 0
    RetryDeferred = 1
    ControlledFailure = 2
    Abort = 3

class DeadlockClass(IntEnum):
    None_ = 0
    CpuDeadlock = 1
    GpuDeadlock = 2
    CpuGpuDeadlock = 3
    MemoryStall = 4
    ResourceStall = 5
    ShaderHang = 6
    HostGpuFailure = 7
    Unknown = 8

MAX_RETRIES = 8

@dataclass
class FaultRecord:
    kind: FaultKind = FaultKind.CpuPageFault
    guest_address: int = 0
    size: int = 0
    access: int = 0
    thread: int = 0
    guest_pc: int = 0
    queue: int = 0
    submit: int = 0
    resource: int = 0
    shader: int = 0
    action: FaultAction = FaultAction.ControlledFailure
    retry_count: int = 0

class FaultManager:
    MAX_RETRIES = MAX_RETRIES
    def __init__(self):
        self.records: list[FaultRecord] = []
        self._resolved = 0
        self._t0 = time.monotonic()

    def record(self, rec: FaultRecord) -> FaultAction:
        r = FaultRecord(**vars(rec))
        if r.retry_count >= self.MAX_RETRIES:
            r.action = FaultAction.ControlledFailure
        elif r.action == FaultAction.Resolved:
            self._resolved += 1
        self.records.append(r)
        if r.action == FaultAction.Resolved and r.retry_count < self.MAX_RETRIES:
            return FaultAction.Resolved
        if r.retry_count >= self.MAX_RETRIES:
            return FaultAction.ControlledFailure
        return r.action

    def metrics(self):
        if not self.records: return {"f faults": 0}
        pages = Counter(a.guest_address & ~0x3FFF for a in self.records)
        mx = max(pages.values())
        return {
            "total": len(self.records),
            "unique_pages": len(pages),
            "same_page_max": mx,
            "resolution_success": self._resolved / len(self.records),
        }

    def classify(self):
        if not self.records: return DeadlockClass.None_
        m = self.metrics()
        if m["same_page_max"] >= 4 and m["resolution_success"] < 0.25:
            return DeadlockClass.MemoryStall
        hangs = sum(1 for r in self.records if r.kind == FaultKind.ShaderHang)
        if hangs * 2 >= len(self.records): return DeadlockClass.ShaderHang
        return DeadlockClass.Unknown

    def clear(self):
        self.records.clear(); self._resolved = 0; self._t0 = time.monotonic()
