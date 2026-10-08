#include "kyty/TraceDiagnostics.h"
#include <sstream>

namespace kyty::infra {

void TraceCollector::Emit(TraceEvent e) { events_.push_back(std::move(e)); }

std::vector<TraceEvent> TraceCollector::Slice(uint64_t frame,
                                              uint64_t submit) const {
  std::vector<TraceEvent> out;
  for (auto& e : events_) {
    if (e.corr.frame == frame && e.corr.gpu_submit == submit) out.push_back(e);
  }
  return out;
}

FailureReport BuildReport(const FaultRecord& fault,
                          const FailureContext& ctx) {
  FailureReport r;
  r.corr = fault.correlation;
  r.guest_pc = fault.guest_pc;
  r.module_symbol = ctx.module_symbol;
  r.abi = ctx.abi;
  {
    std::ostringstream o;
    o << "thread=" << fault.thread << " tls=0x" << std::hex << ctx.tls_base;
    r.thread_tls = o.str();
  }
  {
    std::ostringstream o;
    o << "0x" << std::hex << fault.guest_address << "+0x" << fault.size
      << " acc=" << std::dec << fault.access;
    r.mem_range = o.str();
  }
  r.resource_fp = ctx.resource_fp.empty()
                      ? "res=" + std::to_string(fault.resource)
                      : ctx.resource_fp;
  r.shader_hash = fault.shader;
  {
    std::ostringstream o;
    o << "q=" << fault.queue << " submit=" << fault.submit
      << " retry=" << fault.retry_count;
    r.queue_submit = o.str();
  }
  r.host_result = ctx.host_result;
  r.recovery = ctx.recovery;
  return r;
}

}  // namespace kyty::infra
