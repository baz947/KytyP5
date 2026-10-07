#pragma once
// Module manager + dependency resolver — README §5 (P0).
#include <cstdint>
#include <unordered_map>
#include <vector>
#include "kyty/Compat.h"
#include "kyty/GuestModule.h"

namespace kyty::linker {

class ModuleManager {
 public:
  Result<ModuleId> Discover(const std::string& name, LibraryId lib = 0);
  Result<GuestModule*> Get(ModuleId id);
  Result<const GuestModule*> Get(ModuleId id) const;
  Result<void> SetState(ModuleId id, ModuleState next);
  Result<void> Fail(ModuleId id, RuntimeError reason, std::string detail);
  Result<void> SetParsed(ModuleId id, ParsedElf parsed, uint64_t base,
                         size_t reloc_count, size_t tls_size, size_t tls_align);
  Result<void> AddDependency(ModuleId from, const std::string& needed_name);
  // Topological load order; MissingDependency on unknown/cycle.
  Result<std::vector<ModuleId>> ResolveOrder() const;
  std::vector<ModuleId> All() const;

 private:
  ModuleId next_id_ = 1;
  std::unordered_map<ModuleId, GuestModule> modules_;
  std::unordered_map<ModuleId, std::vector<std::string>> deps_;
  const GuestModule* FindByName(const std::string& n) const;
};

}  // namespace kyty::linker
