#include <cassert>
#include <chrono>
#include <cstdio>
#include <thread>
#include "kyty/WaitManager.h"

int main() {
  using namespace kyty;
  WaitManager wm;
  assert(wm.RegisterObject(7).ok());

  // fast path: condition already true
  assert(wm.Wait(1, 7, 0, std::chrono::milliseconds(10),
                [] { return true; })
             .ok());

  // wake path: background thread wakes waiter
  std::thread waker([&] {
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    wm.Wake(7, 1);
  });
  assert(wm.Wait(2, 7, 10, std::chrono::milliseconds(500),
                [] { return false; })
             .ok());
  waker.join();

  // timeout path -> explicit Timeout error (not return 0)
  auto t = wm.Wait(3, 7, 0, std::chrono::milliseconds(20),
                   [] { return false; });
  assert(!t.ok() && t.error == RuntimeError::Timeout);

  // wait on dead object -> InvalidArgument
  assert(wm.DeleteObject(7).ok());
  auto d = wm.Wait(4, 7, 0, std::chrono::milliseconds(5),
                   [] { return false; });
  assert(!d.ok());

  std::puts("test_wait OK");
  return 0;
}
