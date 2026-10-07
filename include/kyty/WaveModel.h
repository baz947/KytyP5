#pragma once
// Wave32/Wave64 — README §14 (P0). Explicit EXEC/VCC/SCC/lane/scalar.
// IR ops: READ_LANE/WRITE_LANE/BROADCAST/SHUFFLE/BALLOT/ACTIVE_MASK.
// Backends: Native/Split/Software. Wave64 never silently degrades.
#include <cstdint>
#include "kyty/Compat.h"

namespace kyty::gpu {

enum class WaveSize : uint8_t { Wave32 = 32, Wave64 = 64 };
enum class WaveBackend : uint8_t { Native = 0, Split, Software };

struct WaveState {
  WaveSize size = WaveSize::Wave32;
  uint64_t exec = 0;  // active-lane mask (low N bits)
  uint64_t vcc = 0;
  uint32_t scc = 0;
  uint32_t lane_id = 0;
  // cross-lane/scalar summarized as fingerprints
  uint64_t scalar_fp = 0;
  uint64_t cross_lane_fp = 0;
};

class WaveModel {
 public:
  static Result<WaveState> Make(WaveSize size, uint64_t exec);
  // Lower Wave64 for a host capped at 32 lanes: Split (2x32) or Software.
  // Returns Unsupported when silent degradation is requested.
  static Result<WaveBackend> SelectBackend(WaveSize guest, bool host_has_64,
                                           bool allow_split);
  static bool IsLaneActive(const WaveState& w, uint32_t lane);
  static Result<uint64_t> Ballot(const WaveState& w, bool pred);
};

}  // namespace kyty::gpu
