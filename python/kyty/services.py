import math
from .compat import RuntimeError, Result

class VideoOutManager:
    def __init__(self, clock):
        self.clock = clock; self.displays = {}
    def open(self, d, fps=60):
        if d in self.displays and self.displays[d]["open"]:
            return Result.fail(RuntimeError.InvalidArgument, "in use")
        if fps == 0:
            return Result.fail(RuntimeError.InvalidArgument, "fps")
        self.displays[d] = {"open": True, "fps": fps, "vblank": 0,
                            "frame": 1, "last": 0}
        return Result.ok()
    def close(self, d):
        if d not in self.displays or not self.displays[d]["open"]:
            return Result.fail(RuntimeError.InvalidArgument, "not open")
        self.displays[d]["open"] = False
        return Result.ok()
    def flip(self, d, submit):
        dd = self.displays.get(d)
        if not dd or not dd["open"]:
            return Result.fail(RuntimeError.InvalidArgument, "closed")
        now = self.clock.monotonic_ns()
        period = 1_000_000_000 // dd["fps"]
        f = {"id": dd["frame"], "submit": submit, "present": now + period}
        dd["frame"] += 1; dd["vblank"] += 1; dd["last"] = f["present"]
        return Result.ok(f)
    def vblank(self, d): return self.displays.get(d, {}).get("vblank", 0)

class AudioManager:
    def __init__(self, clock):
        self.clock = clock; self.opened = False; self.running = False
        self.q = []; self.pos = 0; self._last = 0; self.rate = 48000
    def open(self, rate=48000, ch=2):
        if not rate or not ch:
            return Result.fail(RuntimeError.InvalidArgument, "cfg")
        self.opened = True; self.rate = rate; self.ch = ch
        return Result.ok()
    def start(self):
        if not self.opened:
            return Result.fail(RuntimeError.InvalidArgument, "open first")
        self.running = True; self._last = self.clock.monotonic_ns()
        return Result.ok()
    def queue(self, pcm):
        if not self.opened:
            return Result.fail(RuntimeError.InvalidArgument, "closed")
        if len(pcm) % self.ch:
            return Result.fail(RuntimeError.InvalidArgument, "channels")
        self.q.append(pcm)
        return Result.ok()
    def poll(self):
        if not self.running:
            return Result.fail(RuntimeError.InvalidArgument, "not running")
        now = self.clock.monotonic_ns()
        due = (now - self._last) * self.rate // 1_000_000_000
        self._last = now
        while due > 0 and self.q:
            n = len(self.q[0]) // self.ch
            if n <= due: due -= n; self.pos += n; self.q.pop(0)
            else: break
        return Result.ok(self.pos)

class PadManager:
    MAX = 4
    def __init__(self): self.slots = {}
    def connect(self, slot, user):
        if not 0 <= slot < self.MAX:
            return Result.fail(RuntimeError.InvalidArgument, "slot")
        if slot in self.slots:
            return Result.fail(RuntimeError.InvalidArgument, "in use")
        self.slots[slot] = {"user": user, "state": {}, "out": {}}
        return Result.ok()
    def submit(self, slot, lx=0.0, ly=0.0):
        if slot not in self.slots:
            return Result.fail(RuntimeError.InvalidArgument, "pad")
        def cl(v):
            if not math.isfinite(v): return 0.0
            return max(-1.0, min(1.0, v))
        self.slots[slot]["state"] = {"lx": cl(lx), "ly": cl(ly)}
        return Result.ok()
    def state(self, slot):
        if slot not in self.slots:
            return Result.fail(RuntimeError.InvalidArgument, "pad")
        return Result.ok(self.slots[slot]["state"])
    def set_output(self, slot, out):
        if slot not in self.slots:
            return Result.fail(RuntimeError.InvalidArgument, "pad")
        self.slots[slot]["out"] = out
        return Result.ok()

class NetworkManager:
    def __init__(self, offline=False):
        self.offline = offline; self._next = 100
        self.open = set(); self.q = {}
    def socket(self):
        if self.offline:
            return Result.fail(RuntimeError.UnsupportedFeature, "offline")
        f = self._next; self._next += 1
        self.open.add(f); self.q[f] = []
        return Result.ok(f)
    def send(self, fd, data):
        if fd not in self.open:
            return Result.fail(RuntimeError.InvalidArgument, "fd")
        self.q[fd].append(bytes(data))
        return Result.ok()
    def recv(self, fd):
        if fd not in self.open:
            return Result.fail(RuntimeError.InvalidArgument, "fd")
        if not self.q[fd]:
            return Result.fail(RuntimeError.Timeout, "empty")
        return Result.ok(self.q[fd].pop(0))

class TrophyManager:
    def __init__(self): self.s = set()
    def unlock(self, user, title, trophy):
        if not user or not title:
            return Result.fail(RuntimeError.InvalidArgument, "scope")
        self.s.add((user, title, trophy))
        return Result.ok()
    def count(self, user, title):
        return sum(1 for u, t, _ in self.s if u == user and t == title)
