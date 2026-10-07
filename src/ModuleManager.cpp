#include "kyty/ModuleManager.h"

namespace kyty::linker {

Result<ModuleId> ModuleManager::Discover(const std::string& name,
                                         LibraryId lib) {
  if (name.empty())
    return Result<ModuleId>::Fail(RuntimeError::InvalidArgument,
                                  "empty module name");
  if (auto* e = FindByName(name))
    return Result<ModuleId>::Ok(e->id);  // idempotent
  ModuleId id = next_id_++;
  GuestModule m;
  m.id = id;
  m.library = lib != 0 ? lib : id;
  m.name = name;
  m.state = ModuleState::Discovered;
  modules_[id] = std::move(m);
  return Result<ModuleId>::Ok(id);
}

Result<GuestModule*> ModuleManager::Get(ModuleId id) {
  auto it = modules_.find(id);
  if (it == modules_.end())
    return Result<GuestModule*>::Fail(RuntimeError::InvalidArgument,
                                      "unknown module");
  return Result<GuestModule*>::Ok(&it->second);
}

Result<const GuestModule*> ModuleManager::Get(ModuleId id) const {
  auto it = modules_.find(id);
  if (it == modules_.end())
    return Result<const GuestModule*>::Fail(RuntimeError::InvalidArgument,
                                            "unknown module");
  return Result<const GuestModule*>::Ok(&it->second);
}

Result<void> ModuleManager::SetState(ModuleId id, ModuleState next) {
  auto it = modules_.find(id);
  if (it == modules_.end())
    return Result<void>::Fail(RuntimeError::InvalidArgument,
                              "unknown module");
  if (!it->second.CanTransitionTo(next))
    return Result<void>::Fail(RuntimeError::InternalInvariant,
                              std::string("illegal lifecycle ") +
                                  ToString(it->second.state) + " -> " +
                                  ToString(next));
  it->second.state = next;
  return Result<void>::Ok();
}

Result<void> ModuleManager::Fail(ModuleId id, RuntimeError reason,
                                 std::string detail) {
  auto it = modules_.find(id);
  if (it == modules_.end())
    return Result<void>::Fail(RuntimeError::InvalidArgument,
                              "unknown module");
  it->second.state = ModuleState::Failed;
  it->second.fail_reason = reason;
  it->second.fail_detail = std::move(detail);
  return Result<void>::Ok();
}

Result<void> ModuleManager::SetParsed(ModuleId id, ParsedElf parsed,
                                      uint64_t base, size_t reloc_count,
                                      size_t tls_size, size_t tls_align) {
  auto it = modules_.find(id);
  if (it == modules_.end())
    return Result<void>::Fail(RuntimeError::InvalidArgument,
                              "unknown module");
  it->second.parsed = std::move(parsed);
  it->second.base_address = base;
  it->second.has_image = true;
  it->second.reloc_count = reloc_count;
  it->second.tls_size = tls_size;
  it->second.tls_align = tls_align ? tls_align : 16;
  if (!it->second.parsed.soname.empty())
    it->second.soname = it->second.parsed.soname;
  return Result<void>::Ok();
}

Result<void> ModuleManager::AddDependency(ModuleId from,
                                         const std::string& needed_name) {
  if (!modules_.count(from))
    return Result<void>::Fail(RuntimeError::InvalidArgument,
                              "unknown module");
  if (needed_name.empty())
    return Result<void>::Fail(RuntimeError::InvalidArgument,
                              "empty DT_NEEDED");
  deps_[from].push_back(needed_name);
  return Result<void>::Ok();
}

Result<std::vector<ModuleId>> ModuleManager::ResolveOrder() const {
  // Kahn over name-resolved edges. Missing -> MissingDependency.
  // Cycle -> MissingDependency (dependency deadlock, not silent).
  std::unordered_map<ModuleId, int> indeg;
  std::unordered_map<ModuleId, std::vector<ModuleId>> edges;
  for (auto& [id, _] : modules_) indeg[id] = 0;
  for (auto& [from, needs] : deps_) {
    for (auto& nm : needs) {
      auto* dep = FindByName(nm);
      if (!dep)
        return Result<std::vector<ModuleId>>::Fail(
            RuntimeError::MissingDependency, "need " + nm);
      edges[dep->id].push_back(from);
      indeg[from]++;
    }
  }
  std::vector<ModuleId> ready;
  for (auto& [id, d] : indeg)
    if (d == 0) ready.push_back(id);
  std::vector<ModuleId> order;
  while (!ready.empty()) {
    ModuleId n = ready.back();
    ready.pop_back();
    order.push_back(n);
    for (auto m : edges[n]) {
      if (--indeg[m] == 0) ready.push_back(m);
    }
  }
  if (order.size() != modules_.size())
    return Result<std::vector<ModuleId>>::Fail(
        RuntimeError::MissingDependency, "dependency cycle");
  return Result<std::vector<ModuleId>>::Ok(std::move(order));
}

std::vector<ModuleId> ModuleManager::All() const {
  std::vector<ModuleId> out;
  for (auto& [id, _] : modules_) out.push_back(id);
  return out;
}

const GuestModule* ModuleManager::FindByName(const std::string& n) const {
  for (auto& [_, m] : modules_) {
    if (m.name == n || (!m.soname.empty() && m.soname == n)) return &m;
  }
  return nullptr;
}

}  // namespace kyty::linker
