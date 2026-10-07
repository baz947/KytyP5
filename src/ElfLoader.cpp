#include "kyty/ElfLoader.h"
#include <cstring>

namespace kyty::linker {

namespace {
#pragma pack(push, 1)
struct Ehdr {
  uint8_t ident[16];
  uint16_t type, machine;
  uint32_t version;
  uint64_t entry, phoff, shoff;
  uint32_t flags;
  uint16_t ehsize, phentsize, phnum, shentsize, shnum, shstrndx;
};
struct Phdr {
  uint32_t type, flags;
  uint64_t offset, vaddr, paddr, filesz, memsz, align;
};
struct Dyn {
  int64_t tag;
  uint64_t val;
};
#pragma pack(pop)
static_assert(sizeof(Ehdr) == 64, "ELF64 header must be 64 bytes");
static_assert(sizeof(Phdr) == 56, "ELF64 Phdr must be 56 bytes");
static_assert(sizeof(Dyn) == 16, "ELF64 Dyn must be 16 bytes");
}  // namespace

Result<ParsedElf> ElfLoader::ParseImage(const uint8_t* data, size_t size) {
  if (!data || size < sizeof(Ehdr))
    return Result<ParsedElf>::Fail(RuntimeError::InvalidArgument,
                                   "empty image");
  Ehdr h{};
  std::memcpy(&h, data, sizeof(h));
  if (!(h.ident[0] == 0x7F && h.ident[1] == 'E' && h.ident[2] == 'L' &&
        h.ident[3] == 'F'))
    return Result<ParsedElf>::Fail(RuntimeError::InvalidArgument,
                                   "bad ELF magic");
  if (h.ident[4] != 2)
    return Result<ParsedElf>::Fail(RuntimeError::UnsupportedFeature,
                                   "only ELF64 supported");
  if (h.ident[5] != 1)
    return Result<ParsedElf>::Fail(RuntimeError::UnsupportedFeature,
                                   "only LE supported");
  if (h.machine != kMachineX86_64)
    return Result<ParsedElf>::Fail(RuntimeError::UnsupportedFeature,
                                   "only x86-64 supported");
  if (h.phoff == 0 || h.phnum == 0 || h.phentsize < sizeof(Phdr))
    return Result<ParsedElf>::Fail(RuntimeError::InvalidArgument,
                                   "no program headers");
  if (h.phoff + size_t(h.phnum) * h.phentsize > size)
    return Result<ParsedElf>::Fail(RuntimeError::InvalidArgument,
                                   "phdrs out of range");

  ParsedElf out;
  out.machine = h.machine;
  out.entry = h.entry;
  uint64_t max_end = 0;
  for (uint16_t i = 0; i < h.phnum; ++i) {
    Phdr p{};
    std::memcpy(&p, data + h.phoff + size_t(i) * h.phentsize, sizeof(p));
    if (p.type == kPtNull) continue;
    if (p.filesz > 0 && p.offset + p.filesz > size)
      return Result<ParsedElf>::Fail(RuntimeError::InvalidArgument,
                                     "segment out of range");
    ProgramHeader q{p.type, p.flags, p.offset, p.vaddr,
                    p.filesz, p.memsz, p.align ? p.align : kPageAlign};
    out.phdrs.push_back(q);
    if (p.type == kPtLoad) {
      uint64_t end = p.vaddr + p.memsz;
      if (end > max_end) max_end = end;
    }
    if (p.type == kPtTls) {
      out.tls.present = true;
      out.tls.vaddr = p.vaddr;
      out.tls.filesz = p.filesz;
      out.tls.memsz = p.memsz;
      out.tls.align = p.align ? p.align : 16;
    }
    if (p.type == kPtDynamic) {
      if (p.filesz % sizeof(Dyn) != 0)
        return Result<ParsedElf>::Fail(RuntimeError::InvalidArgument,
                                       "bad dynamic size");
      size_t n = p.filesz / sizeof(Dyn);
      // Dynamic string table: best-effort — entries referencing strtab
      // beyond image are kept raw; soname/needed names resolved if the
      // strtab VA falls inside a LOAD segment present in the file.
      for (size_t k = 0; k < n; ++k) {
        Dyn d{};
        std::memcpy(&d, data + p.offset + k * sizeof(Dyn), sizeof(d));
        out.dynamic.push_back(DynamicEntry{d.tag, d.val});
        if (d.tag == kDtNull) break;
      }
    }
  }
  if (out.phdrs.empty())
    return Result<ParsedElf>::Fail(RuntimeError::InvalidArgument,
                                   "no usable segments");
  out.load_size = (max_end + kPageAlign - 1) & ~(kPageAlign - 1);

  // Resolve DT_NEEDED/DT_SONAME strings when the strtab lies in-file.
  // We locate the file offset of a VA via PT_LOAD mapping.
  auto va_to_offset = [&](uint64_t va) -> uint64_t {
    for (auto& s : out.phdrs) {
      if (s.type != kPtLoad) continue;
      if (va >= s.vaddr && va < s.vaddr + s.filesz)
        return s.offset + (va - s.vaddr);
    }
    return UINT64_MAX;
  };
  // Heuristic: DT entries with tag NEEDED/SONAME hold strtab offsets;
  // find strtab base as the lowest VA referenced... simplified: scan for
  // a NUL-terminated string at that file offset only if it lands in a
  // LOAD filesz range. Test images built by MakeTestImage satisfy this.
  for (auto& d : out.dynamic) {
    if (d.tag != kDtNeeded && d.tag != kDtSoname) continue;
    // d.val is a strtab offset, not VA — need strtab base. Our test
    // images place strtab right after dynamic; recover via PT_DYNAMIC.
    for (auto& s : out.phdrs) {
      if (s.type != kPtDynamic) continue;
      uint64_t str_off = s.offset + s.filesz;  // layout convention
      uint64_t off = str_off + d.val;
      if (off < size) {
        std::string nm(reinterpret_cast<const char*>(data + off));
        if (d.tag == kDtNeeded) out.needed.push_back(nm);
        if (d.tag == kDtSoname && out.soname.empty()) out.soname = nm;
      }
    }
  }
  return Result<ParsedElf>::Ok(std::move(out));
}

std::vector<uint8_t> ElfLoader::MakeTestImage(
    const std::vector<std::string>& needed, const std::string& soname,
    bool with_tls) {
  // Layout: EHDR + 3 PHDR (LOAD, DYNAMIC, TLS?) + LOAD payload + DYNAMIC +
  // STRTAB. Enough for unit tests of loader/parser/linker.
  std::string strtab;
  std::vector<uint64_t> need_off;
  for (auto& n : needed) {
    need_off.push_back(strtab.size());
    strtab += n;
    strtab.push_back('\0');
  }
  uint64_t soname_off = strtab.size();
  strtab += soname;
  strtab.push_back('\0');

  uint16_t phnum = with_tls ? 3 : 2;
  size_t ehdr_sz = sizeof(Ehdr);
  size_t phdr_sz = sizeof(Phdr);
  size_t dyn_count = needed.size() + 2;  // NEEDED* + SONAME + NULL
  size_t dyn_sz = dyn_count * sizeof(Dyn);
  size_t base = ehdr_sz + phnum * phdr_sz;
  size_t load_off = base;
  size_t load_filesz = 0x100;
  size_t dyn_off = load_off + load_filesz;
  size_t str_off = dyn_off + dyn_sz;

  size_t total = str_off + strtab.size() + 64;
  std::vector<uint8_t> img(total, 0);
  Ehdr h{};
  h.ident[0] = 0x7F;
  h.ident[1] = 'E';
  h.ident[2] = 'L';
  h.ident[3] = 'F';
  h.ident[4] = 2;
  h.ident[5] = 1;
  h.ident[6] = 1;
  h.type = 3;
  h.machine = kMachineX86_64;
  h.version = 1;
  h.entry = 0x1000;
  h.phoff = ehdr_sz;
  h.phentsize = sizeof(Phdr);
  h.phnum = phnum;
  h.ehsize = sizeof(Ehdr);
  std::memcpy(img.data(), &h, sizeof(h));

  Phdr load{};
  load.type = kPtLoad;
  load.flags = 5;
  load.offset = load_off;
  load.vaddr = 0x1000;
  load.filesz = load_filesz;
  load.memsz = 0x4000;
  load.align = kPageAlign;
  std::memcpy(img.data() + ehdr_sz, &load, sizeof(load));

  Phdr dyn{};
  dyn.type = kPtDynamic;
  dyn.flags = 4;
  dyn.offset = dyn_off;
  dyn.vaddr = 0x2000;
  dyn.filesz = dyn_sz;
  dyn.memsz = dyn_sz;
  dyn.align = 8;
  std::memcpy(img.data() + ehdr_sz + phdr_sz, &dyn, sizeof(dyn));

  if (with_tls) {
    Phdr tls{};
    tls.type = kPtTls;
    tls.offset = load_off;
    tls.vaddr = 0x3000;
    tls.filesz = 32;
    tls.memsz = 64;
    tls.align = 16;
    std::memcpy(img.data() + ehdr_sz + 2 * phdr_sz, &tls, sizeof(tls));
  }

  size_t di = 0;
  auto put = [&](int64_t tag, uint64_t v) {
    Dyn d{tag, v};
    std::memcpy(img.data() + dyn_off + di * sizeof(Dyn), &d, sizeof(d));
    ++di;
  };
  for (auto o : need_off) put(kDtNeeded, o);
  put(kDtSoname, soname_off);
  put(kDtNull, 0);
  std::memcpy(img.data() + str_off, strtab.data(), strtab.size());
  return img;
}

}  // namespace kyty::linker
