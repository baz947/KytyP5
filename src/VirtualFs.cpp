#include "kyty/VirtualFs.h"

namespace kyty::svc {

Result<std::string> VirtualFs::Normalize(const std::string& p) {
  if (p.empty() || p[0] != '/')
    return Result<std::string>::Fail(RuntimeError::InvalidArgument,
                                     "guest path must be absolute");
  // Split, resolve "." and reject ".." escape above root.
  std::vector<std::string> parts;
  std::string cur;
  for (size_t i = 1; i <= p.size(); ++i) {
    char c = i < p.size() ? p[i] : '/';
    if (c == '\\')
      return Result<std::string>::Fail(RuntimeError::InvalidArgument,
                                       "host separator in guest path");
    if (c == '/') {
      if (cur == "..") {
        if (parts.empty())
          return Result<std::string>::Fail(RuntimeError::InvalidArgument,
                                           "path escapes root");
        parts.pop_back();
      } else if (!cur.empty() && cur != ".") {
        parts.push_back(cur);
      }
      cur.clear();
    } else {
      cur.push_back(c);
    }
  }
  std::string out = "/";
  for (size_t i = 0; i < parts.size(); ++i) {
    if (i) out += "/";
    out += parts[i];
  }
  return Result<std::string>::Ok(out);
}

Result<void> VirtualFs::Mount(const std::string& guest_prefix,
                              const std::string& backend_tag) {
  (void)backend_tag;
  auto n = Normalize(guest_prefix);
  if (!n.ok()) return Result<void>::Fail(n.error, n.detail);
  mount_prefix_ = n.value;
  dirs_[mount_prefix_] = true;
  return Result<void>::Ok();
}

Result<void> VirtualFs::Mkdir(const std::string& guest_path) {
  auto n = Normalize(guest_path);
  if (!n.ok()) return Result<void>::Fail(n.error, n.detail);
  dirs_[n.value] = true;
  return Result<void>::Ok();
}

Result<void> VirtualFs::WriteFile(const std::string& guest_path,
                                  const std::vector<uint8_t>& data) {
  auto n = Normalize(guest_path);
  if (!n.ok()) return Result<void>::Fail(n.error, n.detail);
  // Ensure parent dir exists (implicit mkdir like PS5 savedata mounts).
  auto slash = n.value.find_last_of('/');
  if (slash != std::string::npos && slash > 0)
    dirs_[n.value.substr(0, slash)] = true;
  files_[n.value] = data;
  return Result<void>::Ok();
}

Result<std::vector<uint8_t>> VirtualFs::ReadFile(
    const std::string& guest_path) const {
  auto n = Normalize(guest_path);
  if (!n.ok())
    return Result<std::vector<uint8_t>>::Fail(n.error, n.detail);
  auto it = files_.find(n.value);
  if (it == files_.end())
    return Result<std::vector<uint8_t>>::Fail(RuntimeError::InvalidArgument,
                                              "no such file");
  return Result<std::vector<uint8_t>>::Ok(it->second);
}

Result<FileStat> VirtualFs::Stat(const std::string& guest_path) const {
  auto n = Normalize(guest_path);
  if (!n.ok()) return Result<FileStat>::Fail(n.error, n.detail);
  auto fi = files_.find(n.value);
  if (fi != files_.end()) {
    FileStat s;
    s.size = fi->second.size();
    return Result<FileStat>::Ok(s);
  }
  if (dirs_.count(n.value)) {
    FileStat s;
    s.is_dir = true;
    return Result<FileStat>::Ok(s);
  }
  return Result<FileStat>::Fail(RuntimeError::InvalidArgument, "no entry");
}

Result<void> VirtualFs::Remove(const std::string& guest_path) {
  auto n = Normalize(guest_path);
  if (!n.ok()) return Result<void>::Fail(n.error, n.detail);
  if (files_.erase(n.value)) return Result<void>::Ok();
  if (dirs_.erase(n.value)) return Result<void>::Ok();
  return Result<void>::Fail(RuntimeError::InvalidArgument, "no entry");
}

Result<std::vector<std::string>> VirtualFs::ListDir(
    const std::string& guest_path) const {
  auto n = Normalize(guest_path);
  if (!n.ok())
    return Result<std::vector<std::string>>::Fail(n.error, n.detail);
  std::string prefix = n.value == "/" ? "/" : n.value + "/";
  std::vector<std::string> out;
  for (auto& [p, _] : files_) {
    if (p.rfind(prefix, 0) == 0) {
      std::string rest = p.substr(prefix.size());
      if (rest.find('/') == std::string::npos) out.push_back(rest);
    }
  }
  return Result<std::vector<std::string>>::Ok(out);
}

}  // namespace kyty::svc
