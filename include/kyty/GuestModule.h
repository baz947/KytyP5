#pragma once
// GuestModule + lifecycle — README §5 (P0).
// A GuestModule owns ELF image meta, memory map, symbols, TLS and reloc state.
#include <cstdint>
#include <string>
#include "kyty/Compat.h"
#include "kyty/ElfLoader.h"

namespace kyty::linker {

using ModuleId = uint64_t;
using LibraryId = uint64_t;

enum class ModuleState : uint8_t {
  Discovered = 0,
  Loading,
  Mapped,
  Parsed,
  DependenciesResolved,
  Relocated,
  Initialized,
  Running,
  Stopping,
  Unloaded,
  Failed,
};

inline const char* ToString(ModuleState s) {
  switch (s) {
    case ModuleState::Discovered: return "Discovered";
    case ModuleState::Loading: return "Loading";
    case ModuleState::Mapped: return "Mapped";
    case ModuleState::Parsed: return "Parsed";
    case ModuleState::DependenciesResolved: return "DependenciesResolved";
    case ModuleState::Relocated: return "Relocated";
    case ModuleState::Initialized: return "Initialized";
    case ModuleState::Running: return "Running";
    case ModuleState::Stopping: return "Stopping";
    case ModuleState::Unloaded: return "Unloaded";
    case ModuleState::Failed: return "Failed";
    default: return "Unknown";
  }
}

struct GuestModule {
  ModuleId id = 0;
  LibraryId library = 0;
  std::string name;    // e.g. "libkernel.sprx"
  std::string soname;  // DT_SONAME when present
  ModuleState state = ModuleState::Discovered;
  uint64_t base_address = 0;  // guest VA where mapped
  ParsedElf parsed;
  bool has_image = false;
  size_t tls_size = 0;
  size_t tls_align = 16;
  size_t reloc_count = 0;
  size_t reloc_applied = 0;
  RuntimeError fail_reason = RuntimeError::None;
  std::string fail_detail;

  bool CanTransitionTo(ModuleState next) const;
};

}  // namespace kyty::linker
