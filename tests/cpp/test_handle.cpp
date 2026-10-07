#include <cassert>
#include <cstdio>
#include "kyty/HandleManager.h"

int main() {
  using namespace kyty;
  HandleManager hm;
  auto h1 = hm.Create(ObjectType::Event);
  auto h2 = hm.Create(ObjectType::Mutex);
  assert(h1.ok() && h2.ok() && h1.value != 0 && h1.value != h2.value);
  assert(hm.Lookup(h1.value).ok());
  assert(!hm.Lookup(0).ok());
  assert(!hm.Lookup(999999).ok());
  assert(hm.Destroy(h1.value).ok());
  assert(!hm.Lookup(h1.value).ok());  // stale after destroy
  assert(hm.Alive() == 1);
  std::puts("test_handle OK");
  return 0;
}
