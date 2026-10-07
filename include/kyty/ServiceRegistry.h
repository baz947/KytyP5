#pragma once
// Service registry — README §10 (P0/P1).
// Every service exposes version, functions, capabilities, dependencies,
// state and diagnostics.
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include "kyty/Compat.h"

namespace kyty::svc {

enum class ServiceState : uint8_t {
  Registered = 0,
  Initialized,
  Running,
  Suspended,
  Failed,
};

struct ServiceInfo {
  std::string name;  // "filesystem", "savedata", ...
  std::string version = "1.0";
  std::vector<std::string> functions;
  std::vector<std::string> capabilities;
  std::vector<std::string> dependencies;
  ServiceState state = ServiceState::Registered;
  CompatStatus compat = CompatStatus::Compatible;
};

class ServiceRegistry {
 public:
  Result<void> Register(ServiceInfo info);
  Result<void> SetState(const std::string& name, ServiceState s);
  Result<ServiceInfo> Lookup(const std::string& name) const;
  Result<void> CheckDependencies() const;  // MissingDependency if unmet
  std::vector<std::string> List() const;

 private:
  std::unordered_map<std::string, ServiceInfo> table_;
};

}  // namespace kyty::svc
