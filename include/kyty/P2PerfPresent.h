#pragma once
// P2: pipeline/view caches + advanced presentation — README P2.
// PipelineCache: LRU keyed on full pipeline_key (shader+IR+res+caps+
// layout+wave+lowering). PresentScheduler: FIFO/MAILBOX/IMMEDIATE,
// VRR/HDR gating, frame pacing + long-run stats (avg/p95/dropped/stalls).
#include <cstdint>
#include <deque>
#include <list>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include "kyty/Compat.h"
#include "kyty/GuestClock.h"

namespace kyty::p2 {

struct CacheStats {
  uint64_t hits = 0, misses = 0, evictions = 0;
  [[nodiscard]] size_t size = 0;
};

class PipelineCache {
 public:
  explicit PipelineCache(size_t capacity = 512) : cap_(capacity) {}
  std::optional<uint64_t> Lookup(const std::string& key);
  void Store(const std::string& key, uint64_t blob);
  [[nodiscard]] CacheStats Stats() const;
  void Clear();

 private:
  size_t cap_;
  mutable std::mutex mu_;
  std::unordered_map<std::string,
                     std::pair<uint64_t, std::list<std::string>::iterator>>
      map_;
  std::list<std::string> lru_;
  CacheStats stats_;
};

enum class PresentMode : uint8_t { Fifo = 0, Mailbox, Immediate };

struct PresentConfig {
  PresentMode mode = PresentMode::Fifo;
  bool vrr = false;
  bool hdr = false;
  uint32_t queue_depth = 2;
  uint32_t target_fps = 60;
};

struct PresentStats {
  uint64_t presented = 0, dropped = 0, stalls = 0;
  double avg_latency_ms = 0.0;
  double p95_latency_ms = 0.0;
};

class PresentScheduler {
 public:
  PresentScheduler(GuestClock* clock, bool host_vrr, bool host_hdr)
      : clock_(clock), host_vrr_(host_vrr), host_hdr_(host_hdr) {}
  Result<void> Configure(PresentConfig cfg);
  Result<void> SubmitFrame(uint64_t submit_id);
  // Presents due frames at now; MAILBOX drops superseded queued frames.
  std::vector<uint64_t> PresentDue();
  [[nodiscard]] PresentStats Stats() const { return stats_; }
  [[nodiscard]] size_t Queued() const { return queue_.size(); }

 private:
  GuestClock* clock_ = nullptr;
  bool host_vrr_ = false, host_hdr_ = false;
  PresentConfig cfg_;
  std::deque<std::pair<uint64_t, uint64_t>> queue_;  // (submit, due_ns)
  PresentStats stats_;
  std::vector<double> latencies_;
  uint64_t last_present_ns_ = 0;
  uint64_t next_id_ = 1;
};

}  // namespace kyty::p2
