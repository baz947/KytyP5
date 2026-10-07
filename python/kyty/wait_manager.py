import threading, time
from enum import IntEnum
from .compat import RuntimeError, Result

class ThreadState(IntEnum):
    Created = 0
    Ready = 1
    Running = 2
    Sleeping = 3
    Waiting = 4
    Suspended = 5
    Signaled = 6
    Terminated = 7

class WaitManager:
    def __init__(self):
        self._mu = threading.Lock()
        self._objects = set()
        self._queues: dict[int, list[dict]] = {}
        self._states = {}

    def register(self, obj):
        if obj == 0: return Result.fail(RuntimeError.InvalidArgument, "object 0")
        with self._mu: self._objects.add(obj)
        return Result.ok()

    def delete(self, obj):
        with self._mu:
            if obj not in self._objects:
                return Result.fail(RuntimeError.InvalidArgument, "no object")
            self._objects.discard(obj); self._queues.pop(obj, None)
        return Result.ok()

    def wait(self, thread, obj, priority, timeout_s, condition):
        if condition and condition(): return Result.ok()
        with self._mu:
            if obj not in self._objects:
                return Result.fail(RuntimeError.InvalidArgument, "dead object")
            self._queues.setdefault(obj, []).append(
                {"thread": thread, "priority": priority, "cancelled": False, "signaled": False})
            self._states[thread] = ThreadState.Waiting
            if condition and condition():
                self._queues[obj] = [w for w in self._queues[obj] if w["thread"] != thread]
                self._states[thread] = ThreadState.Running
                return Result.ok()
        deadline = time.monotonic() + timeout_s
        while time.monotonic() < deadline:
            with self._mu:
                if obj not in self._queues:
                    self._states[thread] = ThreadState.Running
                    return Result.fail(RuntimeError.InvalidArgument, "deleted while waiting")
                ws = [w for w in self._queues[obj] if w["thread"] == thread]
                if ws and ws[0]["cancelled"]:
                    self._queues[obj] = [w for w in self._queues[obj] if w["thread"] != thread]
                    self._states[thread] = ThreadState.Running
                    return Result.fail(RuntimeError.Deadlock, "cancelled")
                if (ws and ws[0]["signaled"]) or (condition and condition()):
                    self._queues[obj] = [w for w in self._queues[obj] if w["thread"] != thread]
                    self._states[thread] = ThreadState.Signaled
                    return Result.ok()
            time.sleep(0.001)
        with self._mu:
            if obj in self._queues:
                self._queues[obj] = [w for w in self._queues[obj] if w["thread"] != thread]
            self._states[thread] = ThreadState.Running
        return Result.fail(RuntimeError.Timeout, "timed out")

    def wake(self, obj, count):
        with self._mu:
            q = self._queues.get(obj, [])
            q.sort(key=lambda w: -w["priority"])
            for i in range(min(count, len(q))): q[i]["signaled"] = True
        return Result.ok()

    def cancel(self, thread):
        with self._mu:
            for q in self._queues.values():
                for w in q:
                    if w["thread"] == thread:
                        w["cancelled"] = True
                        return Result.ok()
        return Result.fail(RuntimeError.InvalidArgument, "not waiting")

    def state(self, thread): return self._states.get(thread, ThreadState.Created)
