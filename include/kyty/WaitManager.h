#pragma once
// Kernel / Scheduler Wait Manager — README §3 (P0).
// Host condvars are blocking mechanisms, NOT the guest scheduler.
// Lost-wakeup-safe: compare -> register atomically -> recheck -> sleep.
// Priority-aware queues + centralized cancellation + lifetime rules.
#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <unordered_map>
#include <vector>
#include "kyty/Compat.h"

namespace kyty {

enum class ThreadState : uint8_t {
  Created = 0,
  Ready,
  Running,
  Sleeping,
  Waiting,
  Suspended,
  Signaled,
  Terminated,
};

struct Waiter {
  uint64_t thread = 0;
  int priority = 0;  // higher wakes first
  bool cancelled = false;
  bool signaled = false;
};

class WaitManager {
 public:
  Result<void> RegisterObject(uint64_t object);
  Result<void> DeleteObject(uint64_t object);

  // Returns Timeout if deadline passes without Wake, Deadlock on cancel.
  Result<void> Wait(uint64_t thread, uint64_t object, int priority,
                    std::chrono::milliseconds timeout,
                    std::function<bool()> condition);

  Result<void> Wake(uint64_t object, size_t count);
  Result<void> Cancel(uint64_t thread);

  [[nodiscard]] ThreadState State(uint64_t thread) const;

 private:
  mutable std::mutex mu_;
  std::unordered_map<uint64_t, bool> objects_;  // object -> exists
  std::unordered_map<uint64_t, std::vector<Waiter>> queues_;
  std::unordered_map<uint64_t, ThreadState> states_;
};

}  // namespace kyty
