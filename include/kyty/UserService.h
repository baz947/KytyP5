#pragma once
// Guest user model — README §10. Single GuestUser feeds Thread/TLS,
// SaveData, Trophy, Controller and NP consistently.
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include "kyty/Compat.h"

namespace kyty::svc {

struct GuestUser {
  uint32_t id = 0;  // PS5 user id, e.g. 10000000
  std::string name;
  bool active = false;
};

class UserService {
 public:
  Result<uint32_t> Create(const std::string& name);
  Result<void> SetActive(uint32_t id);
  Result<GuestUser> Active() const;
  Result<GuestUser> Get(uint32_t id) const;
  std::vector<GuestUser> List() const;

 private:
  uint32_t next_id_ = 10000000;
  std::unordered_map<uint32_t, GuestUser> users_;
  uint32_t active_ = 0;
};

}  // namespace kyty::svc
