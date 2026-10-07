#pragma once
// ELF loader + Dynamic parser — README §5 (P0).
// Minimal ELF64 validation: magic/class/LE/machine; exposes PHDRs + DYNAMIC.
// PS5 is x86-64; page alignment is 16 KiB (kPageSize in GuestMemory).
#include <cstdint>
#include <string>
#include <vector>
#include "kyty/Compat.h"

namespace kyty::linker {

inline constexpr uint32_t kMachineX86_64 = 62;
inline constexpr uint64_t kPageAlign = 0x4000;

// Program header types (generic + SCE range passthrough).
inline constexpr uint32_t kPtNull = 0;
inline constexpr uint32_t kPtLoad = 1;
inline constexpr uint32_t kPtDynamic = 2;
inline constexpr uint32_t kPtTls = 7;

// Dynamic tags we care about.
inline constexpr int64_t kDtNull = 0;
inline constexpr int64_t kDtNeeded = 1;
inline constexpr int64_t kDtSoname = 14;
inline constexpr int64_t kDtRela = 7;
inline constexpr int64_t kDtRelasz = 8;

struct ProgramHeader {
  uint32_t type = kPtNull;
  uint32_t flags = 0;
  uint64_t offset = 0;
  uint64_t vaddr = 0;
  uint64_t filesz = 0;
  uint64_t memsz = 0;
  uint64_t align = 0;
};

struct DynamicEntry {
  int64_t tag = kDtNull;
  uint64_t val = 0;
};

struct TlsTemplateInfo {
  bool present = false;
  uint64_t vaddr = 0;
  uint64_t filesz = 0;
  uint64_t memsz = 0;
  uint64_t align = 16;
};

struct ParsedElf {
  bool is64 = true;
  uint32_t machine = kMachineX86_64;
  uint64_t entry = 0;
  std::vector<ProgramHeader> phdrs;
  std::vector<DynamicEntry> dynamic;
  std::vector<std::string> needed;  // resolved via strtab when available
  std::string soname;
  TlsTemplateInfo tls;
  uint64_t load_size = 0;
};

class ElfLoader {
 public:
  // Parses raw ELF image. Validates magic/class/LE/machine/phdrs/dynamic.
  static Result<ParsedElf> ParseImage(const uint8_t* data, size_t size);
  static Result<ParsedElf> ParseImage(const std::vector<uint8_t>& image) {
    return ParseImage(image.data(), image.size());
  }

  // Builds a minimal valid ELF64 image for tests (one PT_LOAD + optional DYNAMIC).
  static std::vector<uint8_t> MakeTestImage(
      const std::vector<std::string>& needed = {},
      const std::string& soname = "libtest.sprx", bool with_tls = false);
};

}  // namespace kyty::linker
