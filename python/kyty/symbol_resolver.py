from enum import IntEnum
from dataclasses import dataclass
from .compat import RuntimeError, Result

class SymbolType(IntEnum):
    NoType = 0
    Object = 1
    Func = 2
    Tls = 3
    VTable = 4
    FuncPtr = 5
    RuntimeStruct = 6

class SymbolBinding(IntEnum):
    Local = 0
    Global = 1
    Weak = 2

@dataclass(frozen=True)
class SymbolKey:
    library: int
    name: str
    type: SymbolType
    version: str = ""

@dataclass
class SymbolEntry:
    key: SymbolKey
    owner: int
    value: int = 0
    size: int = 0
    binding: SymbolBinding = SymbolBinding.Global
    defined: bool = True

class SymbolResolver:
    def __init__(self):
        self.table: dict[SymbolKey, list[SymbolEntry]] = {}

    def define(self, e: SymbolEntry):
        if not e.key.name:
            return Result.fail(RuntimeError.InvalidArgument, "empty name")
        if not e.defined:
            return Result.fail(RuntimeError.InvalidArgument, "undefined")
        if e.key.type == SymbolType.NoType:
            return Result.fail(RuntimeError.WrongABI, "typeless")
        self.table.setdefault(e.key, []).append(e)
        return Result.ok()

    def _typed(self, key: SymbolKey):
        if not key.name:
            return Result.fail(RuntimeError.InvalidArgument, "empty name")
        if key.type == SymbolType.NoType:
            return Result.fail(RuntimeError.WrongABI, "typeless lookup")
        best = None
        for k, vec in self.table.items():
            if k.name != key.name or k.type != key.type: continue
            if key.version and k.version != key.version: continue
            for e in vec:
                if e.binding == SymbolBinding.Local: continue
                if key.library and e.key.library and e.key.library != key.library:
                    continue
                if best is None or (best.binding == SymbolBinding.Weak
                                     and e.binding == SymbolBinding.Global):
                    best = e
        if best: return Result.ok(best)
        for k in self.table:
            if k.name != key.name or not self.table[k]: continue
            if k.type != key.type:
                return Result.fail(RuntimeError.WrongABI, f"different type: {key.name}")
            if key.version and k.version != key.version:
                return Result.fail(RuntimeError.UnresolvedImport, "version mismatch")
        return Result.fail(RuntimeError.UnresolvedImport, f"no such symbol '{key.name}'")

    def resolve(self, key, requester=0): return self._typed(key)
    def resolve_function(self, lib, name, ver, req=0):
        return self._typed(SymbolKey(lib, name, SymbolType.Func, ver))
    def resolve_object(self, lib, name, ver, req=0):
        return self._typed(SymbolKey(lib, name, SymbolType.Object, ver))
    def resolve_tls(self, lib, name, ver, req=0):
        return self._typed(SymbolKey(lib, name, SymbolType.Tls, ver))
