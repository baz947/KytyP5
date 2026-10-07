from enum import IntEnum
from dataclasses import dataclass
from .compat import Result

class RelocType(IntEnum):
    None_ = 0
    R64 = 1
    GlobDat = 6
    JumpSlot = 7
    Relative = 8
    DtpMod64 = 16
    DtpOff64 = 17
    TpOff64 = 18

class RelocResult(IntEnum):
    Applied = 0
    Deferred = 1
    Stubbed = 2
    Unsupported = 3
    Invalid = 4

@dataclass
class Thunk:
    is_stub: bool = False
    target: int = 0
    policy: str = ""

def apply_one(rtype, offset, addend, target, dep_loaded, load_base,
              allow_stub):
    if offset == 0: return (RelocResult.Invalid, 0, Thunk())
    if rtype == RelocType.Relative:
        return (RelocResult.Applied, load_base + addend, Thunk())
    if rtype in (RelocType.R64, RelocType.GlobDat, RelocType.JumpSlot):
        if target is not None:
            v = target + addend if rtype == RelocType.R64 else target
            return (RelocResult.Applied, v, Thunk(False, target, "direct"))
        if not dep_loaded: return (RelocResult.Deferred, 0, Thunk())
        if allow_stub:
            return (RelocResult.Stubbed, 0, Thunk(True, 0, "safestub:unresolved-import"))
        return (RelocResult.Invalid, 0, Thunk())
    if rtype in (RelocType.DtpMod64, RelocType.DtpOff64, RelocType.TpOff64):
        if target is not None:
            return (RelocResult.Applied, target + addend, Thunk())
        return (RelocResult.Deferred, 0, Thunk())
    return (RelocResult.Unsupported, 0, Thunk())
