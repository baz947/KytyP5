#include "kyty/UserService.h"

namespace kyty::svc {

Result<uint32_t> UserService::Create(const std::string& name) {
  if (name.empty())
    return Result<uint32_t>::Fail(RuntimeError::InvalidArgument,
                                  "empty user name");
  uint32_t id = next_id_++;
  users_[id] = GuestUser{id, name, false};
  if (active_ == 0) {
    active_ = id;
    users_[id].active = true;
  }
  return Result<uint32_t>::Ok(id);
}

Result<void> UserService::SetActive(uint32_t id) {
  auto it = users_.find(id);
  if (it == users_.end())
    return Result<void>::Fail(RuntimeError::InvalidArgument, "no such user");
  if (active_ && users_.count(active_)) users_[active_].active = false;
  active_ = id;
  it->second.active = true;
  return Result<void>::Ok();
}

Result<GuestUser> UserService::Active() const {
  auto it = users_.find(active_);
  if (it == users_.end())
    return Result<GuestUser>::Fail(RuntimeError::MissingDependency,
                                   "no active user");
  return Result<GuestUser>::Ok(it->second);
}

Result<GuestUser> UserService::Get(uint32_t id) const {
  auto it = users_.find(id);
  if (it == users_.end())
    return Result<GuestUser>::Fail(RuntimeError::InvalidArgument,
                                   "no such user");
  return Result<GuestUser>::Ok(it->second);
}

std::vector<GuestUser> UserService::List() const {
  std::vector<GuestUser> out;
  for (auto& [_, u] : users_) out.push_back(u);
  return out;
}

}  // namespace kyty::svc
