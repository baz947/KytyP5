#include "kyty/TraceDiagnostics.h"

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

}  // namespace kyty::infra
