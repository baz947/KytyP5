#pragma once
// Compatibility database + score — README §24/§27 (P1).
// Feature requirements, not game names. A game is evidence for a semantic
// requirement; fixes benefit every game sharing it.
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include "kyty/Compat.h"

namespace kyty::infra {

enum class TitleStatus : uint8_t {
  Boot = 0, Menu, Ingame, Playable,
};

struct TitleRequirements {
  bool wave64 = false;
  bool dynamic_descriptors = false;
  bool ray_tracing = false;
  bool image_atomics = false;
  bool bda = false;
};

struct KnownIssue {
  std::string subsystem;  // "resource_tracking", ...
  CompatStatus status = CompatStatus::Broken;
  std::string note;
};

struct TitleEntry {
  std::string title_id;
  std::string version;
  TitleStatus status = TitleStatus::Boot;
  TitleRequirements requirements;
  std::vector<KnownIssue> issues;
};

// Per-dimension score (Boot/Menu/Gameplay/Rendering/Audio/Input/Save/
// Loading/Stability/Performance), 0..100 each.
struct CompatScore {
  std::map<std::string, uint32_t> dims;
  [[nodiscard]] uint32_t Overall() const;
};

class CompatDatabase {
 public:
  Result<void> Upsert(TitleEntry e);
  Result<TitleEntry> Lookup(const std::string& title_id,
                            const std::string& version) const;
  // Titles needing a feature (regression corpus for that requirement).
  std::vector<TitleEntry> AffectedBy(const std::string& subsystem) const;
  Result<void> SetScore(const std::string& title_id,
                        const std::string& version, CompatScore s);
  Result<CompatScore> Score(const std::string& title_id,
                            const std::string& version) const;
  // Durability: minimal self-describing JSON (titles + scores).
  // Save never loses data silently; Load rejects corrupt files with a
  // structured error and leaves the DB untouched.
  Result<void> Save(const std::string& path) const;
  Result<void> Load(const std::string& path);
  void Clear();

 private:
  static std::string Key(const std::string& t, const std::string& v) {
    return t + "@" + v;
  }
  std::map<std::string, TitleEntry> titles_;
  std::map<std::string, CompatScore> scores_;
};

}  // namespace kyty::infra
