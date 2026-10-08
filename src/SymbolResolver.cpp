#include "kyty/SymbolResolver.h"

namespace kyty::linker {

Result<void> SymbolResolver::Define(SymbolEntry entry) {
  if (entry.key.name.empty())
    return Result<void>::Fail(RuntimeError::InvalidArgument,
                              "empty symbol name");
  if (!entry.defined)
    return Result<void>::Fail(RuntimeError::InvalidArgument,
                              "cannot define undefined symbol");
  if (entry.key.type == SymbolType::NoType)
    return Result<void>::Fail(RuntimeError::WrongABI,
                              "typeless symbol is not a value");
  table_[entry.key].push_back(std::move(entry));
  return Result<void>::Ok();
}

Result<SymbolEntry> SymbolResolver::Resolve(const SymbolKey& key,
                                            ModuleId requester) const {
  (void)requester;
  if (key.name.empty())
    return Result<SymbolEntry>::Fail(RuntimeError::InvalidArgument,
                                     "empty symbol name");
  if (key.type == SymbolType::NoType)
    return Result<SymbolEntry>::Fail(
        RuntimeError::WrongABI,
        "refusing typeless lookup: use Func/Object/Tls path");
  return ResolveTyped(key, requester);
}

Result<SymbolEntry> SymbolResolver::ResolveFunction(
    LibraryId lib, const std::string& name, const std::string& version,
    ModuleId requester) const {
  return ResolveTyped(SymbolKey{lib, name, SymbolType::Func, version},
                      requester);
}

Result<SymbolEntry> SymbolResolver::ResolveObject(
    LibraryId lib, const std::string& name, const std::string& version,
    ModuleId requester) const {
  return ResolveTyped(SymbolKey{lib, name, SymbolType::Object, version},
                      requester);
}

Result<SymbolEntry> SymbolResolver::ResolveTls(
    LibraryId lib, const std::string& name, const std::string& version,
    ModuleId requester) const {
  return ResolveTyped(SymbolKey{lib, name, SymbolType::Tls, version},
                      requester);
}

Result<SymbolEntry> SymbolResolver::ResolveTyped(const SymbolKey& key,
                                                 ModuleId) const {
  // Exact-type match first; Func also accepts FuncPtr/VTable targets?
  // No — callers must pick the right path. A Func import resolving to an
  // Object is WrongABI, not a silent int->ptr cast (README §6).
  // Library scope: when key.library != 0, filter candidates by owner.
  // We iterate all keys with same (name,type,version) across libraries.
  const SymbolEntry* found = nullptr;
  for (auto& [k, vec] : table_) {
    if (k.name != key.name || k.type != key.type) continue;
    // Version: versioned request needs exact; unversioned accepts any
    // (prefers versioned? no — prefers strong binding, deterministic).
    if (!key.version.empty() && k.version != key.version) continue;
    for (auto& e : vec) {
      if (!IsExportable(e.binding)) continue;
      // Library check via owner's library == key.library when scoped.
      // Owner ModuleId doubles as LibraryId unless Discover() overrode it;
      // SymbolEntry.owner is ModuleId; compare against key.library only
      // when the definer recorded library in key.library.
      if (key.library != 0 && e.key.library != 0 &&
          e.key.library != key.library)
        continue;
      if (!found ||
          (found->binding == SymbolBinding::Weak &&
           e.binding == SymbolBinding::Global))
        found = &e;
    }
  }
  if (found) return Result<SymbolEntry>::Ok(*found);

  // Diagnose type confusion explicitly (WrongABI) vs missing (Unresolved).
  for (auto& [k, vec] : table_) {
    if (k.name != key.name || vec.empty()) continue;
    if (k.type != key.type) {
      return Result<SymbolEntry>::Fail(
          RuntimeError::WrongABI,
          "symbol '" + key.name + "' exists as different type");
    }
    if (!key.version.empty() && k.version != key.version) {
      return Result<SymbolEntry>::Fail(
          RuntimeError::UnresolvedImport,
          "symbol '" + key.name + "' version mismatch");
    }
  }
  return Result<SymbolEntry>::Fail(RuntimeError::UnresolvedImport,
                                   "no such symbol '" + key.name + "'");
}

}  // namespace kyty::linker
