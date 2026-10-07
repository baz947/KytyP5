#include "kyty/FaultManager.h"

namespace kyty {

FaultAction FaultManager::Record(const FaultRecord& rec) {
  FaultRecord r = rec;
  // Bounded retry: never infinite. Persistent faults -> ControlledFailure.
  if (r.retry_count >= kMaxRetries) {
    r.action = FaultAction::ControlledFailure;
  } else if (r.action == FaultAction::Resolved) {
    ++resolved_;
  }
  records_.push_back(r);
  const uint64_t page = r.guest_address & ~uint64_t{0x3FFF};  // 16K pages
  per_page_count_[page]++;

  if (r.action == FaultAction::Resolved && r.retry_count < kMaxRetries) {
    return FaultAction::Resolved;
  }
  if (r.retry_count >= kMaxRetries) return FaultAction::ControlledFailure;
  return r.action;
}

StormMetrics FaultManager::Metrics() const {
  StormMetrics m;
  if (records_.empty()) return m;
  auto now = std::chrono::steady_clock::now();
  double secs =
      std::chrono::duration<double>(now - first_).count();
  if (secs < 1e-6) secs = 1e-6;
  m.faults_per_sec = double(records_.size()) / secs;
  m.unique_addresses = per_page_count_.size();
  uint64_t mx = 0;
  for (auto& [_, c] : per_page_count_) mx = c > mx ? c : mx;
  m.same_page_count = mx;
  m.retry_rate = double(records_.size()) / double(records_.size());
  m.resolution_success =
      double(resolved_) / double(records_.size());
  // Stall heuristics: one page dominating + no resolution -> no progress.
  m.cpu_progress = !(mx >= 4 && m.resolution_success < 0.25);
  m.gpu_progress = m.cpu_progress;
  return m;
}

DeadlockClass FaultManager::Classify() const {
  if (records_.empty()) return DeadlockClass::None;
  auto m = Metrics();
  if (!m.cpu_progress && !m.gpu_progress) return DeadlockClass::MemoryStall;
  // Count shader-hang kind
  size_t hangs = 0;
  for (auto& r : records_)
    if (r.kind == FaultKind::ShaderHang) ++hangs;
  if (hangs * 2 >= records_.size()) return DeadlockClass::ShaderHang;
  return DeadlockClass::Unknown;
}

void FaultManager::Clear() {
  records_.clear();
  per_page_count_.clear();
  resolved_ = 0;
  first_ = std::chrono::steady_clock::now();
}

}  // namespace kyty
