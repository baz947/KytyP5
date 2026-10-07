#include "kyty/CompatDb.h"

namespace kyty::infra {

uint32_t CompatScore::Overall() const {
  if (dims.empty()) return 0;
  uint64_t sum = 0;
  for (auto& [_, v] : dims) sum += v;
  return uint32_t(sum / dims.size());
}

Result<void> CompatDatabase::Upsert(TitleEntry e) {
  if (e.title_id.empty())
    return Result<void>::Fail(RuntimeError::InvalidArgument, "title id");
  titles_[Key(e.title_id, e.version)] = std::move(e);
  return Result<void>::Ok();
}

Result<TitleEntry> CompatDatabase::Lookup(const std::string& title_id,
                                          const std::string& version) const {
  auto it = titles_.find(Key(title_id, version));
  if (it == titles_.end())
    return Result<TitleEntry>::Fail(RuntimeError::InvalidArgument,
                                    "unknown title");
  return Result<TitleEntry>::Ok(it->second);
}

std::vector<TitleEntry> CompatDatabase::AffectedBy(
    const std::string& subsystem) const {
  std::vector<TitleEntry> out;
  for (auto& [_, t] : titles_) {
    for (auto& i : t.issues) {
      if (i.subsystem == subsystem) {
        out.push_back(t);
        break;
      }
    }
  }
  return out;
}

Result<void> CompatDatabase::SetScore(const std::string& title_id,
                                      const std::string& version,
                                      CompatScore s) {
  if (!titles_.count(Key(title_id, version)))
    return Result<void>::Fail(RuntimeError::InvalidArgument, "unknown title");
  scores_[Key(title_id, version)] = std::move(s);
  return Result<void>::Ok();
}

Result<CompatScore> CompatDatabase::Score(const std::string& title_id,
                                          const std::string& version) const {
  auto it = scores_.find(Key(title_id, version));
  if (it == scores_.end())
    return Result<CompatScore>::Fail(RuntimeError::InvalidArgument,
                                     "no score");
  return Result<CompatScore>::Ok(it->second);
}

}  // namespace kyty::infra
