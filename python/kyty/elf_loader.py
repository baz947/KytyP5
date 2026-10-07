import struct
from .compat import RuntimeError, Result

MACHINE_X86_64 = 62
PAGE_ALIGN = 0x4000
PT_LOAD, PT_DYNAMIC, PT_TLS = 1, 2, 7

class ParsedElf:
    def __init__(self):
        self.machine = MACHINE_X86_64
        self.entry = 0
        self.phdrs = []
        self.dynamic = []
        self.needed = []
        self.soname = ""
        self.tls_present = False
        self.load_size = 0

def parse_image(data: bytes):
    if len(data) < 64:
        return Result.fail(RuntimeError.InvalidArgument, "empty image")
    magic = data[0:4]
    if magic != b"\x7fELF":
        return Result.fail(RuntimeError.InvalidArgument, "bad ELF magic")
    if data[4] != 2:
        return Result.fail(RuntimeError.UnsupportedFeature, "only ELF64")
    if data[5] != 1:
        return Result.fail(RuntimeError.UnsupportedFeature, "only LE")
    (etype, machine, ver, entry, phoff, shoff, flags, ehsize,
     phentsize, phnum, shentsize, shnum, shstrndx) = struct.unpack_from(
        "<HHIQQQIHHHHHH", data, 16)
    if machine != MACHINE_X86_64:
        return Result.fail(RuntimeError.UnsupportedFeature, "only x86-64")
    if phoff == 0 or phnum == 0:
        return Result.fail(RuntimeError.InvalidArgument, "no program headers")
    out = ParsedElf()
    out.entry = entry
    max_end = 0
    for i in range(phnum):
        o = phoff + i * phentsize
        ptype, pflags, poff, pvaddr, ppaddr, pfilesz, pmemsz, palign = \
            struct.unpack_from("<IIQQQQQQ", data, o)
        if ptype == 0: continue
        if pfilesz and poff + pfilesz > len(data):
            return Result.fail(RuntimeError.InvalidArgument, "segment OOR")
        out.phdrs.append({"type": ptype, "vaddr": pvaddr, "filesz": pfilesz,
                          "memsz": pmemsz, "offset": poff})
        if ptype == PT_LOAD:
            max_end = max(max_end, pvaddr + pmemsz)
        if ptype == PT_TLS:
            out.tls_present = True
    if not out.phdrs:
        return Result.fail(RuntimeError.InvalidArgument, "no segments")
    out.load_size = (max_end + PAGE_ALIGN - 1) & ~(PAGE_ALIGN - 1)
    # dynamic: find PT_DYNAMIC and read entries (tag,val as q,q)
    for p in out.phdrs:
        if p["type"] == PT_DYNAMIC:
            n = p["filesz"] // 16
            for k in range(n):
                tag, val = struct.unpack_from("<qQ", data, p["offset"] + k * 16)
                out.dynamic.append((tag, val))
                if tag == 0: break
    # test-image convention: strtab directly follows dynamic payload
    for p in out.phdrs:
        if p["type"] == PT_DYNAMIC:
            str_off = p["offset"] + p["filesz"]
            for tag, val in out.dynamic:
                if tag in (1, 14):
                    s = data[str_off + val:].split(b"\x00")[0].decode()
                    if tag == 1: out.needed.append(s)
                    elif not out.soname: out.soname = s
    return Result.ok(out)

def make_test_image(needed=None, soname="libtest.sprx", with_tls=False):
    needed = needed or []
    strtab = b""
    offs = []
    for n in needed:
        offs.append(len(strtab)); strtab += n.encode() + b"\x00"
    soname_off = len(strtab); strtab += soname.encode() + b"\x00"
    phnum = 3 if with_tls else 2
    ehdr_sz, phdr_sz = 64, 56
    dyn_count = len(needed) + 2
    dyn_sz = dyn_count * 16
    base = ehdr_sz + phnum * phdr_sz
    load_off, load_filesz = base, 0x100
    dyn_off = load_off + load_filesz
    str_off = dyn_off + dyn_sz
    total = str_off + len(strtab) + 64
    img = bytearray(total)
    img[0:4] = b"\x7fELF"; img[4] = 2; img[5] = 1; img[6] = 1
    struct.pack_into("<HHIQQQIHHHHHH", img, 16, 3, MACHINE_X86_64, 1,
                     0x1000, ehdr_sz, 0, 0, ehdr_sz, phdr_sz, phnum, 0, 0, 0)
    struct.pack_into("<IIQQQQQQ", img, ehdr_sz, PT_LOAD, 5, load_off,
                     0x1000, 0, load_filesz, 0x4000, PAGE_ALIGN)
    struct.pack_into("<IIQQQQQQ", img, ehdr_sz + phdr_sz, PT_DYNAMIC, 4,
                     dyn_off, 0x2000, 0, dyn_sz, dyn_sz, 8)
    if with_tls:
        struct.pack_into("<IIQQQQQQ", img, ehdr_sz + 2 * phdr_sz, PT_TLS,
                         0, load_off, 0x3000, 0, 32, 64, 16)
    di = 0
    for o in offs:
        struct.pack_into("<qQ", img, dyn_off + di * 16, 1, o); di += 1
    struct.pack_into("<qQ", img, dyn_off + di * 16, 14, soname_off); di += 1
    struct.pack_into("<qQ", img, dyn_off + di * 16, 0, 0)
    img[str_off:str_off + len(strtab)] = strtab
    return bytes(img)
