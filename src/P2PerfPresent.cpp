#include "kyty/P2PerfPresent.h"
#include <algorithm>

namespace kyty::p2 {

std::optional<uint64_t> PipelineCache::Lookup(const std::string& key) {
  std::lock_guard<std::mutex> lk(mu_);
  auto it = map_.find(key);
  if (it == map_.end()) {
    ++stats_.misses;
    return std::nullopt;
  }
  ++stats_.hits;
  lru_.erase(it->second.second);
  lru_.push_front(key);
  it->second.second = lru_.begin();
  return it->second.first;
}

void PipelineCache::Store(const std::string& key, uint64_t blob) {
  std::lock_guard<std::mutex> lk(mu_);
  auto it = map_.find(key);
  if (it != map_.end()) {
    it->second.first = blob;
    lru_.erase(it->second.second);
    lru_.push_front(key);
    it->second.second = lru_.begin();
    return;
  }
  if (map_.size() >= cap_ && cap_ > 0) {
    auto victim = lru_.back();
    lru_.pop_back();
    map_.erase(victim);
    ++stats_.evictions;
  }
  lru_.push_front(key);
  map_[key] = {blob, lru_.begin()};
  stats_.size = map_.size();
}

CacheStats PipelineCache::Stats() const {
  std::lock_guard<std::mutex> lk(mu_);
  CacheStats s = stats_;
  s.size = map_.size();
  return s;
}

void PipelineCache::Clear() {
  std::lock_guard<std::mutex> lk(mu_);
  map_.clear();
  lru_.clear();
  stats_ = CacheStats{};
}

Result<void> PresentScheduler::Configure(PresentConfig cfg) {
  if (cfg.target_fps == 0 || cfg.queue_depth == 0)
    return Result<void>::Fail(RuntimeError::InvalidArgument, "bad present cfg");
  if (cfg.vrr && !host_vrr_)
    return Result<void>::Fail(RuntimeError::UnsupportedFeature,
                              "VRR not on host");
  if (cfg.hdr && !host_hdr_)
    return Result<void>::Fail(RuntimeError::UnsupportedFeature,
                              "HDR not on host");
  if (cfg.mode == PresentMode::Immediate && cfg.queue_depth > 1)
    return Result<void>::Fail(RuntimeError::InvalidArgument,
                              "immediate needs depth 1");
  cfg_ = cfg;
  return Result<void>::Ok();
}

Result<void> PresentScheduler::SubmitFrame(uint64_t submit_id) {
  if (submit_id == 0)
    return Result<void>::Fail(RuntimeError::InvalidArgument, "bad submit");
  uint64_t now = clock_ ? clock_->MonotonicNs() : 0;
  uint64_t period = 1'000'000'000ull / cfg_.target_fps;
  queue_.emplace_back(submit_id, now + period);
  // Bound the queue: MAILBOX drops oldest superseded, FIFO drops oldest.
  while (queue_.size() > cfg_.queue_depth) {
    queue_.pop_front();
    ++stats_.dropped;
  }
  (void)next_id_;
  return Result<void>::Ok();
}

std::vector<uint64_t> PresentScheduler::PresentDue() {
  std::vector<uint64_t> out;
  uint64_t now = clock_ ? clock_->MonotonicNs() : 0;
  uint64_t period = 1'000'000'000ull / (cfg_.target_fps ? cfg_.target_fps : 60);
  while (!queue_.empty() && queue_.front().second <= now) {
    auto [submit, due] = queue_.front();
    queue_.pop_front();
    // MAILBOX: if newer frames already due, skip superseded ones.
    if (cfg_.mode == PresentMode::Mailbox) {
      while (!queue_.empty() && queue_.front().second <= now) {
        ++stats_.dropped;
        queue_.pop_front();
      }
    }
    double latency = double(now >= due ? now - due : 0) / 1e6;
    latencies_.push_back(latency);
    if (last_present_ns_ && now - last_present_ns_ > 2 * period)
      ++stats_.stalls;
    last_present_ns_ = now;
    ++stats_.presented;
    out.push_back(submit);
    if (cfg_.mode == PresentMode::Mailbox) break;  // one newest per tick
  }
  if (!latencies_.empty()) {
    double sum = 0;
    for (auto v : latencies_) sum += v;
    stats_.avg_latency_ms = sum / double(latencies_.size());
    std::vector<double> s = latencies_;
    std::sort(s.begin(), s.end());
    stats_.p95_latency_ms = s[(s.size() * 95) / 100];
  }
  return out;
}

}  // namespace kyty::p2
