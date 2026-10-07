#pragma once
// SaveData — README §10.
// Guest SaveData API -> Manager -> Virtual SaveData FS -> Host backend.
// Transactional: writes stage in a pending overlay, Commit atomically
// applies, quota enforced, user+title scoped.
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include "kyty/Compat.h"

namespace kyty::svc {

struct SaveMount {
  uint32_t user_id = 0;
  std::string title_id;
  std::string mount_point;  // e.g. "/savedata/10000000/PPSAxxxxx"
  uint64_t quota_bytes = 64ull * 1024 * 1024;
};

class SaveDataManager {
 public:
  Result<SaveMount> Mount(uint32_t user_id, const std::string& title_id,
                          uint64_t quota = 64ull * 1024 * 1024);
  Result<void> Stage(const SaveMount& m, const std::string& rel,
                     const std::vector<uint8_t>& data);
  Result<std::vector<uint8_t>> Read(const SaveMount& m,
                                    const std::string& rel) const;
  Result<void> Commit(const SaveMount& m);    // atomic apply
  Result<void> Rollback(const SaveMount& m);  // drop staged
  [[nodiscard]] uint64_t Usage(const SaveMount& m) const;

 private:
  static std::string Key(const SaveMount& m, const std::string& rel);
  static Result<void> CheckRel(const std::string& rel);
  std::map<std::string, std::vector<uint8_t>> committed_;
  std::map<std::string, std::vector<uint8_t>> staged_;
  std::map<std::string, uint64_t> quota_;
};

}  // namespace kyty::svc
