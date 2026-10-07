#pragma once
// Diagnostics trace — README §26 (P0).
// Every important op shares correlation identity; failures reconstructable
// from guest PC/module/ABI/thread-TLS/memory/resource/shader/queue/host.
#include <cstdint>
#include <string>
#include <vector>
#include "kyty/Compat.h"
#include "kyty/Diagnostics.h"

namespace kyty::infra {

struct TraceEvent {
  Correlation corr;
  std::string subsystem;  // "memory","linker","gpu","service",...
  std::string message;
  RuntimeError error = RuntimeError::None;
  uint64_t guest_pc = 0;
  std::string module;
};

struct FailureReport {
  Correlation corr;
  uint64_t guest_pc = 0;
  std::string module_symbol;
  std::string abi;
  std::string thread_tls;
  std::string mem_range;
  std::string resource_fp;
  uint64_t shader_hash = 0;
  std::string queue_submit;
  std::string host_result;
  std::string recovery;
};

class TraceCollector {
 public:
  void Emit(TraceEvent e);
  // Correlated slice: all events sharing frame+submit (cross-subsystem).
  [[nodiscard]] std::vector<TraceEvent> Slice(uint64_t frame,
                                              uint64_t submit) const;
  [[nodiscard]] size_t Size() const { return events_.size(); }
  void Clear() { events_.clear(); }

 private:
  std::vector<TraceEvent> events_;
};

}  // namespace kyty::infra
