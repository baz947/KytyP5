#pragma once
// Virtual filesystem — README §10.
// Guest FS -> Virtual FS -> Host Backend.
// Host paths and host error codes never leak into guest semantics.
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include "kyty/Compat.h"

namespace kyty::svc {

struct FileStat {
  bool is_dir = false;
  uint64_t size = 0;
};

class VirtualFs {
 public:
  Result<void> Mount(const std::string& guest_prefix,
                     const std::string& backend_tag);
  Result<void> Mkdir(const std::string& guest_path);
  Result<void> WriteFile(const std::string& guest_path,
                         const std::vector<uint8_t>& data);
  Result<std::vector<uint8_t>> ReadFile(const std::string& guest_path) const;
  Result<FileStat> Stat(const std::string& guest_path) const;
  Result<void> Remove(const std::string& guest_path);
  Result<std::vector<std::string>> ListDir(const std::string& guest_path) const;

 private:
  // Normalized guest path: absolute, no ".." escape, no host separators.
  static Result<std::string> Normalize(const std::string& p);
  std::map<std::string, std::vector<uint8_t>> files_;
  std::map<std::string, bool> dirs_;  // path -> true
  std::string mount_prefix_ = "/app0";
};

}  // namespace kyty::svc
