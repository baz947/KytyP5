#include "kyty/HandleManager.h"

namespace kyty {

Result<uint32_t> HandleManager::Create(ObjectType type) {
  if (next_ == 0) ++next_;  // skip 0
  if (next_ == 0xFFFFFFFF)
    return Result<uint32_t>::Fail(RuntimeError::HostMemoryFailure,
                                  "handle space exhausted");
  uint32_t h = next_++;
  table_[h] = ObjectEntry{type, 1, true};
  return Result<uint32_t>::Ok(h);
}

Result<ObjectEntry> HandleManager::Lookup(uint32_t handle) const {
  if (handle == 0)
    return Result<ObjectEntry>::Fail(RuntimeError::InvalidArgument,
                                     "handle 0 invalid");
  auto it = table_.find(handle);
  if (it == table_.end() || !it->second.alive)
    return Result<ObjectEntry>::Fail(RuntimeError::InvalidArgument,
                                     "stale handle");
  return Result<ObjectEntry>::Ok(it->second);
}

Result<void> HandleManager::Destroy(uint32_t handle) {
  auto it = table_.find(handle);
  if (it == table_.end() || !it->second.alive)
    return Result<void>::Fail(RuntimeError::InvalidArgument,
                              "destroy stale handle");
  it->second.alive = false;
  table_.erase(it);
  return Result<void>::Ok();
}

size_t HandleManager::Alive() const { return table_.size(); }

}  // namespace kyty
