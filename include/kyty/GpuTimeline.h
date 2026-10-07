#pragma once
// GPU timeline / scheduler / lifetime — README §22 (P0).
// Guest Timeline -> Master Timeline -> Vulkan Timeline Semaphore.
// Lifetime obeys GPU completion, not guest handle release.
// Cache keys carry full fingerprints.
#include <cstdint>
#include <string>
#include <unordered_map>
#include "kyty/Compat.h"

namespace kyty::gpu {

struct GpuSubmit {
  uint64_t guest_id = 0;
  uint64_t host_id = 0;
  uint32_t queue = 0;
  uint64_t shader_hash = 0;
  std::string resource_key;
};

class GpuTimeline {
 public:
  Result<uint64_t> Submit(uint64_t guest_id, uint32_t queue,
                          uint64_t shader_hash, std::string resource_key);
  Result<void> Complete(uint64_t host_id);  // GPU completion signal
  [[nodiscard]] bool IsComplete(uint64_t host_id) const;
  [[nodiscard]] uint64_t GuestToHost(uint64_t guest_id) const;
  // Resource may be destroyed only after its last submit completes.
  [[nodiscard]] bool CanDestroy(const std::string& resource_key) const;

  static std::string BufferKey(uint64_t addr, uint64_t range,
                               uint64_t mem_version, uint64_t desc_fp,
                               const std::string& extra);
  static std::string PipelineKey(uint64_t shader_hash, uint64_t ir_version,
                                 uint64_t res_model, uint64_t host_caps_hash,
                                 const std::string& layout_fp,
                                 const std::string& wave_mode);

 private:
  uint64_t next_host_ = 1;
  std::unordered_map<uint64_t, GpuSubmit> by_host_;
  std::unordered_map<uint64_t, uint64_t> guest2host_;
};

}  // namespace kyty::gpu
