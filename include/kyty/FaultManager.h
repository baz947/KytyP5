#pragma once
// Fault Manager / Deadlock Detector — README §23
// Every serious fault becomes a FaultRecord. Persistent faults are
// controlled failures, never infinite retry.
#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include "kyty/Compat.h"
#include "kyty/Diagnostics.h"

namespace kyty {

enum class FaultKind : uint8_t {
  CpuPageFault = 0,
  GpuPageFault,
  BdaFault,
  ResourceFault,
  ShaderHang,
  HostGpuFailure,
  Timeout,
  Deadlock,
};

enum class FaultAction : uint8_t {
  Resolved = 0,      // mapping fixed, retry allowed (bounded)
  RetryDeferred,     // wait for dependency then retry
  ControlledFailure, // persistent fault -> structured error, no more retry
  Abort,             // host-level fatal
};

enum class DeadlockClass : uint8_t {
  None = 0,
  CpuDeadlock,
  GpuDeadlock,
  CpuGpuDeadlock,
  MemoryStall,
  ResourceStall,
  ShaderHang,
  HostGpuFailure,
  Unknown,
};

struct FaultRecord {
  FaultKind kind = FaultKind::CpuPageFault;
  uint64_t guest_address = 0;
  uint64_t size = 0;
  uint32_t access = 0;  // bit0 read, bit1 write, bit2 exec
  uint64_t thread = 0;
  uint64_t guest_pc = 0;
  uint64_t queue = 0;
  uint64_t submit = 0;
  uint64_t resource = 0;
  uint64_t shader = 0;
  FaultAction action = FaultAction::ControlledFailure;
  uint32_t retry_count = 0;
  Correlation correlation{};
};

struct StormMetrics {
  double faults_per_sec = 0.0;
  size_t unique_addresses = 0;
  uint64_t same_page_count = 0;
  double retry_rate = 0.0;
  double resolution_success = 0.0;
  bool cpu_progress = true;
  bool gpu_progress = true;
};

class FaultManager {
 public:
  static constexpr uint32_t kMaxRetries = 8;

  FaultManager() = default;

  // Record a fault. Returns the action to take (bounded retry enforced).
  FaultAction Record(const FaultRecord& rec);

  [[nodiscard]] StormMetrics Metrics() const;
  [[nodiscard]] size_t TotalFaults() const { return records_.size(); }
  [[nodiscard]] const std::vector<FaultRecord>& Records() const {
    return records_;
  }

  // Dependency-graph stub for deadlock classification (P0 minimal).
  // Nodes: GuestThread/GpuQueue/Resource/Timeline/KernelObject (README §23).
  // Returns Unknown unless an obvious stall pattern is present.
  [[nodiscard]] DeadlockClass Classify() const;

  void Clear();

 private:
  std::vector<FaultRecord> records_;
  std::unordered_map<uint64_t, uint64_t> per_page_count_;
  uint64_t resolved_ = 0;
  std::chrono::steady_clock::time_point first_ =
      std::chrono::steady_clock::now();
};

}  // namespace kyty
