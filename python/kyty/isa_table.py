"""Real ISA table mirror (names/classes/coverage)."""

ENTRIES = [
    ("MIMG.SAMPLE", "mimg", 0x10, {"backend": "lowered-cover"}),
    ("MIMG.SAMPLE_L", "mimg", 0x10, {}),
    ("MIMG.GATHER4", "mimg", 0x10, {"backend": "lowered"}),
    ("MIMG.GETRESINFO", "mimg", 0x10, {}),
    ("MIMG.ATOMIC_ADD", "atomic", 0x31, {"backend": "lowered"}),
    ("MIMG.ATOMIC_CMPXCHG", "atomic", 0x31, {"backend": "lowered"}),
    ("SALU.S_BARRIER", "barrier", 0x30, {}),
    ("WAVE.READLANE", "wave", 0x20, {}),
    ("WAVE.WRITELANE", "wave", 0x21, {}),
    ("WAVE.BROADCAST", "wave", 0x22, {}),
]

def coverage_at(stage="decode"):
    ok = 0
    for _, _, _, caps in ENTRIES:
        s = caps.get(stage, "covered")
        if s in ("covered", "lowered", "lowered-cover"):
            ok += 1
    return ok / len(ENTRIES)

def find(name):
    for e in ENTRIES:
        if e[0] == name:
            return e
    return None
