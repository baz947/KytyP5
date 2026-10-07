#include "kyty/FuzzChaos.h"

namespace kyty::infra {

uint64_t SeedRandom::Next() {
  uint64_t x = s_;
  x ^= x << 13;
  x ^= x >> 7;
  x ^= x << 17;
  s_ = x;
  return x ? x : 0x9E3779B97F4A7C15ull;
}

void SeedRandom::Fill(uint8_t* out, size_t n) {
  for (size_t i = 0; i < n; ++i) out[i] = uint8_t(Next() >> 33);
}

FuzzCase Fuzzer::MutateElf(uint64_t seed) {
  SeedRandom r(seed);
  FuzzCase c{seed, "elf", std::vector<uint8_t>(128)};
  r.Fill(c.bytes.data(), c.bytes.size());
  // Keep ELF magic sometimes so the loader exercises deeper paths.
  if (r.Range(2)) {
    c.bytes[0] = 0x7F;
    c.bytes[1] = 'E';
    c.bytes[2] = 'L';
    c.bytes[3] = 'F';
  }
  return c;
}

FuzzCase Fuzzer::MutateDescriptor(uint64_t seed) {
  SeedRandom r(seed);
  FuzzCase c{seed, "descriptor", std::vector<uint8_t>(64)};
  r.Fill(c.bytes.data(), c.bytes.size());
  return c;
}

FuzzCase Fuzzer::MutateShaderWords(uint64_t seed, size_t count) {
  SeedRandom r(seed);
  FuzzCase c{seed, "shader-words", {}};
  c.bytes.resize(count * 4);
  r.Fill(c.bytes.data(), c.bytes.size());
  return c;
}

FuzzCase Fuzzer::MutateGuestPointer(uint64_t seed) {
  SeedRandom r(seed);
  FuzzCase c{seed, "guest-pointer", std::vector<uint8_t>(16)};
  r.Fill(c.bytes.data(), c.bytes.size());
  return c;
}

bool ChaosPlan::Has(ChaosFault f) const {
  for (auto x : faults)
    if (x == f) return true;
  return false;
}

Result<void> ChaosInjector::BeforeGpuSubmit() const {
  if (plan_.Has(ChaosFault::DeviceLost))
    return Result<void>::Fail(RuntimeError::HostGpuFailure,
                              "chaos: device lost");
  if (plan_.Has(ChaosFault::GpuDelay))
    return Result<void>::Fail(RuntimeError::Timeout, "chaos: gpu delay");
  return Result<void>::Ok();
}

Result<void> ChaosInjector::BeforeMemAccess() const {
  if (plan_.Has(ChaosFault::MemoryFault))
    return Result<void>::Fail(RuntimeError::InvalidGuestMemory,
                              "chaos: memory fault");
  if (plan_.Has(ChaosFault::HostAllocFailure))
    return Result<void>::Fail(RuntimeError::HostMemoryFailure,
                              "chaos: alloc failure");
  return Result<void>::Ok();
}

Result<void> ChaosInjector::BeforeDescriptorBind() const {
  if (plan_.Has(ChaosFault::DescriptorMiss))
    return Result<void>::Fail(RuntimeError::InvalidResource,
                              "chaos: descriptor miss");
  return Result<void>::Ok();
}

}  // namespace kyty::infra
