from dataclasses import dataclass
from enum import IntEnum
from typing import Generic, TypeVar

T = TypeVar("T")

class CompatStatus(IntEnum):
    Exact = 0
    Compatible = 1
    Emulated = 2
    Partial = 3
    SafeStub = 4
    Unsupported = 5
    WrongABI = 6
    Broken = 7

class GpuCapability(IntEnum):
    Native = 0
    Lowered = 1
    Emulated = 2
    Fallback = 3
    Unavailable = 4

class RuntimeError(IntEnum):
    None_ = 0
    InvalidArgument = 1
    InvalidGuestMemory = 2
    MissingDependency = 3
    UnresolvedImport = 4
    WrongABI = 5
    UnsupportedFeature = 6
    InvalidResource = 7
    ShaderFailure = 8
    HostMemoryFailure = 9
    HostGpuFailure = 10
    Timeout = 11
    Deadlock = 12
    InternalInvariant = 13

@dataclass
class Result(Generic[T]):
    error: RuntimeError = RuntimeError.None_
    value: object = None
    detail: str = ""

    @staticmethod
    def ok(value=None):
        return Result(RuntimeError.None_, value, "")

    @staticmethod
    def fail(error, detail=""):
        return Result(error, None, detail)

    def is_ok(self):
        return self.error == RuntimeError.None_
