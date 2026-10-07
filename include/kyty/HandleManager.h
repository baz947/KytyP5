#pragma once
// Handle / Object Manager — README §28 item 5 (P0).
// Unified lifetime: guest handles never alias raw host pointers.
#include <cstdint>
#include <unordered_map>
#include "kyty/Compat.h"

namespace kyty {

enum class ObjectType : uint8_t {
  Thread = 0,
  Event,
  Mutex,
  Semaphore,
  File,
  Socket,
  GpuResource,
  VideoOut,
};

struct ObjectEntry {
  ObjectType type = ObjectType::Event;
  uint64_t version = 0;
  bool alive = true;
};

class HandleManager {
 public:
  // Returns guest handle (>=1). 0 is never valid.
  Result<uint32_t> Create(ObjectType type);
  Result<ObjectEntry> Lookup(uint32_t handle) const;
  Result<void> Destroy(uint32_t handle);
  [[nodiscard]] size_t Alive() const;

 private:
  uint32_t next_ = 1;
  std::unordered_map<uint32_t, ObjectEntry> table_;
};

}  // namespace kyty
