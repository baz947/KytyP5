#pragma once
// Symbol resolution — README §6 (P0).
// Key is (library, name, type, version), NOT just name->address.
// Function imports and object imports are separate paths; TLS/vtable/
// funcptr/runtime-structs are never generic integers.
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include "kyty/Compat.h"
#include "kyty/GuestModule.h"

namespace kyty::linker {

enum class SymbolType : uint8_t {
  NoType = 0,
  Object,       // data object
  Func,         // code
  Tls,          // thread-local object (separate path)
  VTable,       // C++ vtable (func-ptr table, not int)
  FuncPtr,      // function pointer value
  RuntimeStruct,// runtime metadata struct
};

enum class SymbolBinding : uint8_t {
  Local = 0,
  Global,
  Weak,
};

struct SymbolKey {
  LibraryId library = 0;  // 0 = any library (search order), else scoped
  std::string name;
  SymbolType type = SymbolType::NoType;
  std::string version;    // "" = unversioned request (accepts any)

  bool operator==(const SymbolKey& o) const {
    return library == o.library && name == o.name && type == o.type &&
           version == o.version;
  }
};

struct SymbolKeyHash {
  size_t operator()(const SymbolKey& k) const noexcept {
    size_t h = std::hash<std::string>{}(k.name);
    h ^= std::hash<LibraryId>{}(k.library) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<int>{}(int(k.type)) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<std::string>{}(k.version) + 0x9e3779b9 + (h << 6) +
         (h >> 2);
    return h;
  }
};

struct SymbolEntry {
  SymbolKey key;
  ModuleId owner = 0;
  uint64_t value = 0;  // guest VA (resolved target)
  uint64_t size = 0;
  SymbolBinding binding = SymbolBinding::Global;
  bool defined = true;
};

class SymbolResolver {
 public:
  Result<void> Define(SymbolEntry entry);
  // Generic resolve honoring library scope + version + type separation.
  Result<SymbolEntry> Resolve(const SymbolKey& key,
                              ModuleId requester) const;
  // Separate paths (must be used by callers, not merged):
  Result<SymbolEntry> ResolveFunction(LibraryId lib, const std::string& name,
                                      const std::string& version,
                                      ModuleId requester) const;
  Result<SymbolEntry> ResolveObject(LibraryId lib, const std::string& name,
                                    const std::string& version,
                                    ModuleId requester) const;
  Result<SymbolEntry> ResolveTls(LibraryId lib, const std::string& name,
                                 const std::string& version,
                                 ModuleId requester) const;

 private:
  // WTO: weak vs global preference. Local symbols are never exported.
  static bool IsExportable(SymbolBinding b) {
    return b == SymbolBinding::Global || b == SymbolBinding::Weak;
  }
  Result<SymbolEntry> ResolveTyped(const SymbolKey& key,
                                   ModuleId requester) const;
  std::unordered_map<SymbolKey, std::vector<SymbolEntry>, SymbolKeyHash>
      table_;
};

}  // namespace kyty::linker
