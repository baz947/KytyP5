#include "kyty/SaveData.h"

namespace kyty::svc {

std::string SaveDataManager::Key(const SaveMount& m, const std::string& rel) {
  return std::to_string(m.user_id) + "|" + m.title_id + "|" + rel;
}

Result<void> SaveDataManager::CheckRel(const std::string& rel) {
  if (rel.empty() || rel[0] == '/' || rel.find("..") != std::string::npos)
    return Result<void>::Fail(RuntimeError::InvalidArgument,
                              "bad save rel path");
  return Result<void>::Ok();
}

Result<SaveMount> SaveDataManager::Mount(uint32_t user_id,
                                         const std::string& title_id,
                                         uint64_t quota) {
  if (user_id == 0 || title_id.empty())
    return Result<SaveMount>::Fail(RuntimeError::InvalidArgument,
                                   "bad user/title");
  SaveMount m{user_id, title_id,
              "/savedata/" + std::to_string(user_id) + "/" + title_id, quota};
  quota_[Key(m, "")] = quota;
  return Result<SaveMount>::Ok(m);
}

Result<void> SaveDataManager::Stage(const SaveMount& m, const std::string& rel,
                                    const std::vector<uint8_t>& data) {
  if (auto c = CheckRel(rel); !c.ok()) return c;
  // Quota over staged+committed for this mount.
  uint64_t used = Usage(m);
  if (used + data.size() > m.quota_bytes)
    return Result<void>::Fail(RuntimeError::HostMemoryFailure,
                              "savedata quota exceeded");
  staged_[Key(m, rel)] = data;
  return Result<void>::Ok();
}

Result<std::vector<uint8_t>> SaveDataManager::Read(
    const SaveMount& m, const std::string& rel) const {
  if (auto c = CheckRel(rel); !c.ok())
    return Result<std::vector<uint8_t>>::Fail(c.error, c.detail);
  auto it = staged_.find(Key(m, rel));
  if (it != staged_.end())
    return Result<std::vector<uint8_t>>::Ok(it->second);
  auto jt = committed_.find(Key(m, rel));
  if (jt != committed_.end())
    return Result<std::vector<uint8_t>>::Ok(jt->second);
  return Result<std::vector<uint8_t>>::Fail(RuntimeError::InvalidArgument,
                                            "no such save file");
}

Result<void> SaveDataManager::Commit(const SaveMount& m) {
  // Atomic: move all staged keys of this mount.
  std::string scope = std::to_string(m.user_id) + "|" + m.title_id + "|";
  for (auto& [k, v] : staged_) {
    if (k.rfind(scope, 0) == 0) committed_[k] = v;
  }
  for (auto it = staged_.begin(); it != staged_.end();) {
    if (it->first.rfind(scope, 0) == 0)
      it = staged_.erase(it);
    else
      ++it;
  }
  return Result<void>::Ok();
}

Result<void> SaveDataManager::Rollback(const SaveMount& m) {
  std::string scope = std::to_string(m.user_id) + "|" + m.title_id + "|";
  for (auto it = staged_.begin(); it != staged_.end();) {
    if (it->first.rfind(scope, 0) == 0)
      it = staged_.erase(it);
    else
      ++it;
  }
  return Result<void>::Ok();
}

uint64_t SaveDataManager::Usage(const SaveMount& m) const {
  uint64_t total = 0;
  std::string scope = std::to_string(m.user_id) + "|" + m.title_id + "|";
  for (auto& [k, v] : committed_) {
    if (k.rfind(scope, 0) == 0) total += v.size();
  }
  for (auto& [k, v] : staged_) {
    if (k.rfind(scope, 0) == 0) total += v.size();
  }
  return total;
}

}  // namespace kyty::svc
