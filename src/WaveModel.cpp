#include "kyty/WaveModel.h"

namespace kyty::gpu {

Result<WaveState> WaveModel::Make(WaveSize size, uint64_t exec) {
  uint64_t lanes = size == WaveSize::Wave32 ? 32 : 64;
  uint64_t full = lanes == 64 ? ~0ull : ((1ull << 32) - 1);
  if ((exec & ~full) != 0)
    return Result<WaveState>::Fail(RuntimeError::InvalidArgument,
                                   "EXEC exceeds wave size");
  if (exec == 0)
    return Result<WaveState>::Fail(RuntimeError::InvalidArgument,
                                   "empty EXEC");
  WaveState w;
  w.size = size;
  w.exec = exec;
  w.vcc = exec;
  return Result<WaveState>::Ok(w);
}

Result<WaveBackend> WaveModel::SelectBackend(WaveSize guest, bool host_has_64,
                                             bool allow_split) {
  if (guest == WaveSize::Wave32) return Result<WaveBackend>::Ok(WaveBackend::Native);
  if (host_has_64) return Result<WaveBackend>::Ok(WaveBackend::Native);
  if (allow_split) return Result<WaveBackend>::Ok(WaveBackend::Split);
  // No silent Wave64->Wave32 degradation: caller must pick Software
  // emulation explicitly or fail Unsupported.
  return Result<WaveBackend>::Fail(RuntimeError::UnsupportedFeature,
                                   "wave64 needs explicit lowering");
}

bool WaveModel::IsLaneActive(const WaveState& w, uint32_t lane) {
  uint64_t lanes = w.size == WaveSize::Wave32 ? 32 : 64;
  if (lane >= lanes) return false;
  return (w.exec >> lane) & 1ull;
}

Result<uint64_t> WaveModel::Ballot(const WaveState& w, bool pred) {
  (void)pred;
  return Result<uint64_t>::Ok(w.exec);
}

}  // namespace kyty::gpu
