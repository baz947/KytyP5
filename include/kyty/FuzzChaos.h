#pragma once
// Fuzz + chaos — README §25 (P1).
// Fuzz: ELF/dynamic/symbols/relocs/descriptors/formats/shaders/CFG/IR/
// guest-pointers/handles/waits. Every crash -> reproducible seed.
// Chaos: deterministic GPU delay/memory fault/descriptor miss/thread delay/
// timeout/device lost/host alloc failure.
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include "kyty/Compat.h"

namespace kyty::infra {

// Deterministic xorshift64 PRNG (fixed seeds -> reproducible corpus).
class SeedRandom {
 public:
  explicit SeedRandom(uint64_t seed) : s_(seed ? seed : 0x9E3779B97F4A7C15ull) {}
  uint64_t Next();
  uint64_t Range(uint64_t n) { return Next() % (n ? n : 1); }
  void Fill(uint8_t* out, size_t n);

 private:
  uint64_t s_;
};

struct FuzzCase {
  uint64_t seed = 0;
  std::string target;  // "elf","descriptor","reloc","shader-words",...
  std::vector<uint8_t> bytes;
};

// Mutators produce byte blobs; harnesses interpret them strictly
// (invalid input -> structured error, never crash).
class Fuzzer {
 public:
  static FuzzCase MutateElf(uint64_t seed);
  static FuzzCase MutateDescriptor(uint64_t seed);
  static FuzzCase MutateShaderWords(uint64_t seed, size_t count = 8);
  static FuzzCase MutateGuestPointer(uint64_t seed);
};

enum class ChaosFault : uint8_t {
  None = 0,
  GpuDelay,
  MemoryFault,
  DescriptorMiss,
  ThreadDelay,
  Timeout,
  DeviceLost,
  HostAllocFailure,
};

struct ChaosPlan {
  std::vector<ChaosFault> faults;
  [[nodiscard]] bool Has(ChaosFault f) const;
};

class ChaosInjector {
 public:
  explicit ChaosInjector(ChaosPlan plan) : plan_(std::move(plan)) {}
  // Each hook returns a structured error when its fault is armed.
  Result<void> BeforeGpuSubmit() const;
  Result<void> BeforeMemAccess() const;
  Result<void> BeforeDescriptorBind() const;
  [[nodiscard]] const ChaosPlan& plan() const { return plan_; }

 private:
  ChaosPlan plan_;
};

}  // namespace kyty::infra
