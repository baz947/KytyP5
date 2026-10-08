"""Full RDNA table + P2 cache/present mirror."""

FULL = (
    [f"MIMG.{n}" for n in ("SAMPLE", "SAMPLE_P", "SAMPLE_L", "SAMPLE_LB",
                            "SAMPLE_GATHER4", "GETRESINFO", "LD", "ST",
                            "ATOMIC_SWAP", "ATOMIC_ADD", "ATOMIC_SMIN",
                            "ATOMIC_UMIN", "ATOMIC_SMAX", "ATOMIC_UMAX",
                            "ATOMIC_AND", "ATOMIC_OR", "ATOMIC_XOR",
                            "ATOMIC_INC", "ATOMIC_DEC", "ATOMIC_CMPXCHG")]
    + [f"BUF.{n}" for n in ("LOAD_FMT", "STORE_FMT", "LOAD", "STORE",
                             "ATOMIC_ADD", "ATOMIC_CMPXCHG")]
    + ["SMEM.LOAD", "SMEM.STORE", "SOPP.S_BARRIER", "SOPP.S_NOP",
       "SALU.S_AND_SAVEEXEC", "SALU.S_MOV_B64", "SALU.S_BALLOT"]
    + [f"VOP.{n}" for n in ("MOV", "ADD_F32", "MUL_F32", "MAD_F32",
                             "MAD_F64", "FLOOR", "CMP_EQ", "CMP_LT")]
    + ["DS.ADD_U32", "DS.XCHG", "DS.CMPST", "DS.BARRIER",
       "FLAT.LOAD", "FLAT.STORE", "FLAT.ATOMIC_ADD",
       "WAVE.READLANE", "WAVE.WRITELANE", "WAVE.BROADCAST",
       "FMT.BC_DECOMPRESS"]
    + ["SOP1.S_MOV_B32", "SOP2.S_ADD_U32", "SOPK.S_MOVK_I32",
       "SOPC.S_CMP_EQ_U32", "VOP3P.V_PK_ADD_F16", "VOP3P.V_PK_MUL_F16",
       "VINTRP.V_INTERP_P1_F32", "VINTRP.V_INTERP_P2_F32", "EXP.EXPORT"]
)


class LruCache:
    def __init__(self, cap=512):
        self.cap = cap; self.d = {}; self.order = []
        self.hits = self.misses = self.evicts = 0
    def lookup(self, k):
        if k not in self.d:
            self.misses += 1
            return None
        self.hits += 1
        self.order.remove(k); self.order.insert(0, k)
        return self.d[k]
    def store(self, k, v):
        if k in self.d:
            self.d[k] = v
            self.order.remove(k); self.order.insert(0, k)
            return
        if len(self.d) >= self.cap and self.cap:
            self.d.pop(self.order.pop())
            self.evicts += 1
        self.d[k] = v
        self.order.insert(0, k)


class PresentSim:
    def __init__(self, fps=60, mode="fifo", depth=2):
        self.fps = fps; self.mode = mode; self.depth = depth
        self.q = []; self.presented = 0; self.dropped = 0
        self.stalls = 0; self._last = 0; self.now = 0
    def submit(self, sid):
        if not sid: return False
        period = 1_000_000_000 // self.fps
        self.q.append((sid, self.now + period))
        while len(self.q) > self.depth:
            self.q.pop(0); self.dropped += 1
        return True
    def advance(self, ns): self.now += ns
    def present_due(self):
        out = []
        period = 1_000_000_000 // self.fps
        while self.q and self.q[0][1] <= self.now:
            sid, due = self.q.pop(0)
            if self.mode == "mailbox":
                while self.q and self.q[0][1] <= self.now:
                    self.q.pop(0); self.dropped += 1
            if self._last and self.now - self._last > 2 * period:
                self.stalls += 1
            self._last = self.now
            self.presented += 1
            out.append(sid)
            if self.mode == "mailbox": break
        return out
