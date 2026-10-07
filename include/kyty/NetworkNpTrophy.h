#pragma once
// Network / NP / Trophy / SystemParams — README §10.
// Translated sockets+errors; NP uses deterministic backends
// (Offline/Local/LAN/Mock), never proprietary Sony infra.
// Trophy is transactional and user/title aware.
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "kyty/Compat.h"

namespace kyty::svc {

enum class NetBackend : uint8_t { Offline = 0, Local, Lan, Mock };

struct NetEndpoint {
  std::string addr;  // guest-visible form, e.g. "10.0.0.2:9307"
  NetBackend backend = NetBackend::Mock;
};

class NetworkManager {
 public:
  explicit NetworkManager(NetBackend b = NetBackend::Mock) : backend_(b) {}
  Result<int> Socket();  // guest fd >= 100
  Result<void> Connect(int fd, NetEndpoint ep);
  Result<void> Send(int fd, const std::vector<uint8_t>& data);
  Result<std::vector<uint8_t>> Recv(int fd);
  Result<void> Close(int fd);
  void SetBackend(NetBackend b) { backend_ = b; }

 private:
  NetBackend backend_ = NetBackend::Mock;
  int next_fd_ = 100;
  std::map<int, std::vector<std::vector<uint8_t>>> loopback_;
  std::set<int> open_;
};

struct TrophyId {
  uint32_t user_id = 0;
  std::string title_id;
  uint32_t trophy = 0;
  bool operator<(const TrophyId& o) const {
    if (user_id != o.user_id) return user_id < o.user_id;
    if (title_id != o.title_id) return title_id < o.title_id;
    return trophy < o.trophy;
  }
};

class TrophyManager {
 public:
  Result<void> Unlock(const TrophyId& t);  // transactional per (user,title)
  [[nodiscard]] bool IsUnlocked(const TrophyId& t) const;
  [[nodiscard]] size_t Count(uint32_t user, const std::string& title) const;

 private:
  std::set<TrophyId> unlocked_;
};

struct SystemParams {
  std::string language = "en-US";
  std::string region = "US";
  uint32_t user_id = 10000000;
  std::string title_id = "PPSA00000";
};

}  // namespace kyty::svc
