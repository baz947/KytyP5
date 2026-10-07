from enum import IntEnum
from dataclasses import dataclass, field
from .compat import RuntimeError, Result

class ModuleState(IntEnum):
    Discovered = 0
    Loading = 1
    Mapped = 2
    Parsed = 3
    DependenciesResolved = 4
    Relocated = 5
    Initialized = 6
    Running = 7
    Stopping = 8
    Unloaded = 9
    Failed = 10

_LINEAR = {
    ModuleState.Discovered: (ModuleState.Loading,),
    ModuleState.Loading: (ModuleState.Mapped,),
    ModuleState.Mapped: (ModuleState.Parsed,),
    ModuleState.Parsed: (ModuleState.DependenciesResolved,),
    ModuleState.DependenciesResolved: (ModuleState.Relocated,),
    ModuleState.Relocated: (ModuleState.Initialized,),
    ModuleState.Initialized: (ModuleState.Running,),
    ModuleState.Running: (ModuleState.Stopping,),
    ModuleState.Stopping: (ModuleState.Unloaded,),
}

@dataclass
class GuestModule:
    id: int = 0
    library: int = 0
    name: str = ""
    soname: str = ""
    state: ModuleState = ModuleState.Discovered
    base_address: int = 0
    fail_reason: RuntimeError = RuntimeError.None_
    fail_detail: str = ""

    def can_transition(self, nxt) -> bool:
        if self.state in (ModuleState.Failed, ModuleState.Unloaded):
            return False
        if nxt == ModuleState.Failed:
            return True
        return nxt in _LINEAR.get(self.state, ())

class ModuleManager:
    def __init__(self):
        self._next = 1
        self.modules: dict[int, GuestModule] = {}
        self.deps: dict[int, list[str]] = {}

    def discover(self, name, lib=0):
        if not name:
            return Result.fail(RuntimeError.InvalidArgument, "empty name")
        for m in self.modules.values():
            if m.name == name or m.soname == name:
                return Result.ok(m.id)
        i = self._next; self._next += 1
        self.modules[i] = GuestModule(id=i, library=lib or i, name=name)
        return Result.ok(i)

    def set_state(self, i, nxt):
        m = self.modules.get(i)
        if not m: return Result.fail(RuntimeError.InvalidArgument, "unknown")
        if not m.can_transition(nxt):
            return Result.fail(RuntimeError.InternalInvariant, "illegal lifecycle")
        m.state = nxt
        return Result.ok()

    def fail(self, i, reason, detail=""):
        m = self.modules.get(i)
        if not m: return Result.fail(RuntimeError.InvalidArgument, "unknown")
        m.state = ModuleState.Failed
        m.fail_reason = reason; m.fail_detail = detail
        return Result.ok()

    def add_dependency(self, frm, need):
        if frm not in self.modules:
            return Result.fail(RuntimeError.InvalidArgument, "unknown")
        if not need:
            return Result.fail(RuntimeError.InvalidArgument, "empty DT_NEEDED")
        self.deps.setdefault(frm, []).append(need)
        return Result.ok()

    def _by_name(self, n):
        for m in self.modules.values():
            if m.name == n or m.soname == n: return m
        return None

    def resolve_order(self):
        indeg = {i: 0 for i in self.modules}
        edges: dict[int, list[int]] = {i: [] for i in self.modules}
        for frm, needs in self.deps.items():
            for nm in needs:
                d = self._by_name(nm)
                if not d:
                    return Result.fail(RuntimeError.MissingDependency, f"need {nm}")
                edges[d.id].append(frm); indeg[frm] += 1
        ready = [i for i, d in indeg.items() if d == 0]
        order = []
        while ready:
            n = ready.pop()
            order.append(n)
            for m in edges[n]:
                indeg[m] -= 1
                if indeg[m] == 0: ready.append(m)
        if len(order) != len(self.modules):
            return Result.fail(RuntimeError.MissingDependency, "cycle")
        return Result.ok(order)
