#include "kyty/WaitManager.h"
#include <algorithm>
#include <thread>

namespace kyty {

Result<void> WaitManager::RegisterObject(uint64_t object) {
  std::lock_guard<std::mutex> lk(mu_);
  if (object == 0)
    return Result<void>::Fail(RuntimeError::InvalidArgument, "object 0");
  objects_[object] = true;
  return Result<void>::Ok();
}

Result<void> WaitManager::DeleteObject(uint64_t object) {
  std::lock_guard<std::mutex> lk(mu_);
  if (!objects_.count(object))
    return Result<void>::Fail(RuntimeError::InvalidArgument, "no object");
  objects_.erase(object);
  queues_.erase(object);  // lifetime rule: waiters on deleted object cancel
  return Result<void>::Ok();
}

Result<void> WaitManager::Wait(uint64_t thread, uint64_t object, int priority,
                               std::chrono::milliseconds timeout,
                               std::function<bool()> condition) {
  // 1. compare (fast path)
  if (condition && condition()) return Result<void>::Ok();
  {
    std::lock_guard<std::mutex> lk(mu_);
    if (!objects_.count(object))
      return Result<void>::Fail(RuntimeError::InvalidArgument,
                                "wait on dead object");
    // 2. register atomically
    queues_[object].push_back(Waiter{thread, priority, false, false});
    states_[thread] = ThreadState::Waiting;
    // 3. recheck (lost-wakeup-safe)
    if (condition && condition()) {
      auto& q = queues_[object];
      q.erase(std::remove_if(q.begin(), q.end(),
                             [&](const Waiter& w) {
                               return w.thread == thread;
                             }),
              q.end());
      states_[thread] = ThreadState::Running;
      return Result<void>::Ok();
    }
  }
  // 4. sleep with timeout (host blocking primitive only here)
  auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    {
      std::lock_guard<std::mutex> lk(mu_);
      auto it = queues_.find(object);
      bool gone = (it == queues_.end());
      bool cancelled = false, signaled = false;
      if (!gone) {
        for (auto& w : it->second)
          if (w.thread == thread) {
            cancelled = w.cancelled;
            signaled = w.signaled;
          }
      }
      if (gone) {
        states_[thread] = ThreadState::Running;
        return Result<void>::Fail(RuntimeError::InvalidArgument,
                                  "object deleted while waiting");
      }
      if (cancelled) {
        auto& q = it->second;
        q.erase(std::remove_if(q.begin(), q.end(),
                               [&](const Waiter& w) {
                                 return w.thread == thread;
                               }),
                q.end());
        states_[thread] = ThreadState::Running;
        return Result<void>::Fail(RuntimeError::Deadlock, "cancelled");
      }
      if (signaled || (condition && condition())) {
        auto& q = it->second;
        q.erase(std::remove_if(q.begin(), q.end(),
                               [&](const Waiter& w) {
                                 return w.thread == thread;
                               }),
                q.end());
        states_[thread] = ThreadState::Signaled;
        return Result<void>::Ok();
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  // Timeout path: remove waiter
  {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = queues_.find(object);
    if (it != queues_.end()) {
      auto& q = it->second;
      q.erase(std::remove_if(q.begin(), q.end(),
                             [&](const Waiter& w) {
                               return w.thread == thread;
                             }),
              q.end());
    }
    states_[thread] = ThreadState::Running;
  }
  return Result<void>::Fail(RuntimeError::Timeout, "wait timed out");
}

Result<void> WaitManager::Wake(uint64_t object, size_t count) {
  std::lock_guard<std::mutex> lk(mu_);
  auto it = queues_.find(object);
  if (it == queues_.end()) return Result<void>::Ok();  // no waiters
  auto& q = it->second;
  // Priority-aware: highest first
  std::sort(q.begin(), q.end(),
            [](const Waiter& a, const Waiter& b) {
              return a.priority > b.priority;
            });
  for (size_t i = 0; i < count && i < q.size(); ++i) q[i].signaled = true;
  return Result<void>::Ok();
}

Result<void> WaitManager::Cancel(uint64_t thread) {
  std::lock_guard<std::mutex> lk(mu_);
  bool found = false;
  for (auto& [_, q] : queues_)
    for (auto& w : q)
      if (w.thread == thread) {
        w.cancelled = true;
        found = true;
      }
  if (!found)
    return Result<void>::Fail(RuntimeError::InvalidArgument,
                              "thread not waiting");
  return Result<void>::Ok();
}

ThreadState WaitManager::State(uint64_t thread) const {
  std::lock_guard<std::mutex> lk(mu_);
  auto it = states_.find(thread);
  if (it == states_.end()) return ThreadState::Created;
  return it->second;
}

}  // namespace kyty
