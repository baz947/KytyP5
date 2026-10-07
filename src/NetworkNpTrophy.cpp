#include "kyty/NetworkNpTrophy.h"

namespace kyty::svc {

Result<int> NetworkManager::Socket() {
  if (backend_ == NetBackend::Offline)
    return Result<int>::Fail(RuntimeError::UnsupportedFeature,
                             "network offline");
  int fd = next_fd_++;
  open_.insert(fd);
  return Result<int>::Ok(fd);
}

Result<void> NetworkManager::Connect(int fd, NetEndpoint ep) {
  if (!open_.count(fd))
    return Result<void>::Fail(RuntimeError::InvalidArgument, "bad fd");
  if (ep.addr.empty())
    return Result<void>::Fail(RuntimeError::InvalidArgument, "bad endpoint");
  (void)ep;
  return Result<void>::Ok();
}

Result<void> NetworkManager::Send(int fd,
                                  const std::vector<uint8_t>& data) {
  if (!open_.count(fd))
    return Result<void>::Fail(RuntimeError::InvalidArgument, "bad fd");
  if (backend_ == NetBackend::Offline)
    return Result<void>::Fail(RuntimeError::UnsupportedFeature, "offline");
  // Mock/LAN loopback: deliver to own recv queue (deterministic).
  loopback_[fd].push_back(data);
  return Result<void>::Ok();
}

Result<std::vector<uint8_t>> NetworkManager::Recv(int fd) {
  if (!open_.count(fd))
    return Result<std::vector<uint8_t>>::Fail(RuntimeError::InvalidArgument,
                                              "bad fd");
  auto& q = loopback_[fd];
  if (q.empty())
    return Result<std::vector<uint8_t>>::Fail(RuntimeError::Timeout,
                                              "no data (mock)");
  auto v = q.front();
  q.erase(q.begin());
  return Result<std::vector<uint8_t>>::Ok(v);
}

Result<void> NetworkManager::Close(int fd) {
  if (!open_.count(fd))
    return Result<void>::Fail(RuntimeError::InvalidArgument, "bad fd");
  open_.erase(fd);
  loopback_.erase(fd);
  return Result<void>::Ok();
}

Result<void> TrophyManager::Unlock(const TrophyId& t) {
  if (t.user_id == 0 || t.title_id.empty())
    return Result<void>::Fail(RuntimeError::InvalidArgument,
                              "bad trophy scope");
  unlocked_.insert(t);  // idempotent, transactional per key
  return Result<void>::Ok();
}

bool TrophyManager::IsUnlocked(const TrophyId& t) const {
  return unlocked_.count(t) != 0;
}

size_t TrophyManager::Count(uint32_t user,
                            const std::string& title) const {
  size_t n = 0;
  for (auto& t : unlocked_) {
    if (t.user_id == user && t.title_id == title) ++n;
  }
  return n;
}

}  // namespace kyty::svc
