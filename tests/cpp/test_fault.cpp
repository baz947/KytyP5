#include <cassert>
#include <cstdio>
#include "kyty/FaultManager.h"

int main() {
  using namespace kyty;
  FaultManager fm;
  // bounded retry: kMaxRetries then ControlledFailure
  for (uint32_t i = 0; i < FaultManager::kMaxRetries + 2; ++i) {
    FaultRecord rec;
    rec.kind = FaultKind::BdaFault;
    rec.guest_address = 0x10000;
    rec.retry_count = i;
    rec.action = FaultAction::Resolved;
    auto a = fm.Record(rec);
    if (i >= FaultManager::kMaxRetries)
      assert(a == FaultAction::ControlledFailure);
  }
  assert(fm.TotalFaults() == FaultManager::kMaxRetries + 2);
  auto m = fm.Metrics();
  assert(m.same_page_count == fm.TotalFaults());
  // storm with no resolution progress -> classified as stall, not hang
  assert(fm.Classify() != DeadlockClass::None);

  fm.Clear();
  assert(fm.TotalFaults() == 0);
  assert(fm.Classify() == DeadlockClass::None);

  std::puts("test_fault OK");
  return 0;
}
