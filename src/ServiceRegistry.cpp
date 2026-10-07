#include "kyty/ServiceRegistry.h"

namespace kyty::svc {

Result<void> ServiceRegistry::Register(ServiceInfo info) {
  if (info.name.empty())
    return Result<void>::Fail(RuntimeError::InvalidArgument, "empty name");
  if (table_.count(info.name))
    return Result<void>::Fail(RuntimeError::InvalidArgument,
                              "already registered");
  table_[info.name] = std::move(info);
  return Result<void>::Ok();
}

Result<void> ServiceRegistry::SetState(const std::string& name,
                                       ServiceState s) {
  auto it = table_.find(name);
  if (it == table_.end())
    return Result<void>::Fail(RuntimeError::MissingDependency, name);
  it->second.state = s;
  return Result<void>::Ok();
}

Result<ServiceInfo> ServiceRegistry::Lookup(const std::string& name) const {
  auto it = table_.find(name);
  if (it == table_.end())
    return Result<ServiceInfo>::Fail(RuntimeError::MissingDependency, name);
  return Result<ServiceInfo>::Ok(it->second);
}

Result<void> ServiceRegistry::CheckDependencies() const {
  for (auto& [n, info] : table_) {
    for (auto& d : info.dependencies) {
      if (!table_.count(d))
        return Result<void>::Fail(RuntimeError::MissingDependency,
                                  n + " needs " + d);
    }
  }
  return Result<void>::Ok();
}

std::vector<std::string> ServiceRegistry::List() const {
  std::vector<std::string> out;
  for (auto& [n, _] : table_) out.push_back(n);
  return out;
}

}  // namespace kyty::svc
