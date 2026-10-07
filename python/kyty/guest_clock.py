import time

class GuestClock:
    TICKS_PER_SEC = 1_000_000_000
    def __init__(self):
        self._boot = time.monotonic()
        self._injected = 0
        self._ticks = 0
    def monotonic_ns(self):
        return int((time.monotonic() - self._boot) * 1e9) + self._injected
    def real_ns(self):
        return int(time.time() * 1e9) + self._injected
    def process_ns(self): return self.monotonic_ns()
    def thread_ns(self, tid): return self.monotonic_ns()
    def guest_ticks(self): return self._ticks + self.monotonic_ns()
    def advance_ns(self, d):
        self._injected += d; self._ticks += d
