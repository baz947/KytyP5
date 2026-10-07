#include "kyty/GpuTimeline.h"
#include <sstream>

namespace kyty::gpu {

Result<uint64_t> GpuTimeline::Submit(uint64_t guest_id, uint32_t queue,
                                     uint64_t shader_hash,
                                     std::string resource_key) {
  if (guest_id == 0)
    return Result<uint64_t>::Fail(RuntimeError::InvalidArgument,
                                  "bad guest submit");
  uint64_t host = next_host_++;
  by_host_[host] = GpuSubmit{guest_id, host, queue, shader_hash,
                             std::move(resource_key)};
  guest2host_[guest_id] = host;
  return Result<uint64_t>::Ok(host);
}

Result<void> GpuTimeline::Complete(uint64_t host_id) {
  auto it = by_host_.find(host_id);
  if (it == by_host_.end())
    return Result<void>::Fail(RuntimeError::InvalidArgument, "bad submit");
  by_host_.erase(it);  // completed: lifetime released
  return Result<void>::Ok();
}

bool GpuTimeline::IsComplete(uint64_t host_id) const {
  return by_host_.find(host_id) == by_host_.end();
}

uint64_t GpuTimeline::GuestToHost(uint64_t guest_id) const {
  auto it = guest2host_.find(guest_id);
  return it == guest2host_.end() ? 0 : it->second;
}

bool GpuTimeline::CanDestroy(const std::string& resource_key) const {
  for (auto& [_, s] : by_host_) {
    if (s.resource_key == resource_key) return false;  // still in flight
  }
  return true;
}

std::string GpuTimeline::BufferKey(uint64_t addr, uint64_t range,
                                    uint64_t mem_version, uint64_t desc_fp,
                                    const std::string& extra) {
  std::ostringstream o;
  o << std::hex << addr << ":" << range << ":" << mem_version << ":"
    << desc_fp << ":" << extra;
  return o.str();
}

std::string GpuTimeline::PipelineKey(uint64_t shader_hash,
                                      uint64_t ir_version, uint64_t res_model,
                                      uint64_t host_caps_hash,
                                      const std::string& layout_fp,
                                      const std::string& wave_mode) {
  std::ostringstream o;
  o << std::hex << shader_hash << ":" << ir_version << ":" << res_model
    << ":" << host_caps_hash << ":" << layout_fp << ":" << wave_mode;
  return o.str();
}

}  // namespace kyty::gpu
