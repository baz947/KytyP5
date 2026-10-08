#include "kyty/PadManager.h"
#include <cmath>

namespace kyty::svc {

float PadManager::ClampAxis(float v) {
  if (!std::isfinite(v)) return 0.0f;
  if (v > 1.0f) return 1.0f;
  if (v < -1.0f) return -1.0f;
  return v;
}

Result<void> PadManager::Connect(int slot, uint32_t user_id) {
  if (slot < 0 || slot >= kMaxPads)
    return Result<void>::Fail(RuntimeError::InvalidArgument, "bad slot");
  auto& s = slots_[size_t(slot)];
  if (s.connected)
    return Result<void>::Fail(RuntimeError::InvalidArgument,
                              "slot in use");
  s.connected = true;
  s.user_id = user_id;
  s.state = PadState{};
  s.state.connected_users = 1u << uint32_t(slot);
  return Result<void>::Ok();
}

Result<void> PadManager::Disconnect(int slot) {
  if (slot < 0 || slot >= kMaxPads)
    return Result<void>::Fail(RuntimeError::InvalidArgument, "bad slot");
  slots_[size_t(slot)] = Slot{};
  return Result<void>::Ok();
}

Result<void> PadManager::SubmitHostInput(int slot, PadState raw) {
  if (slot < 0 || slot >= kMaxPads)
    return Result<void>::Fail(RuntimeError::InvalidArgument, "bad slot");
  auto& s = slots_[size_t(slot)];
  if (!s.connected)
    return Result<void>::Fail(RuntimeError::InvalidArgument,
                              "pad not connected");
  raw.lx = ClampAxis(raw.lx);
  raw.ly = ClampAxis(raw.ly);
  raw.rx = ClampAxis(raw.rx);
  raw.ry = ClampAxis(raw.ry);
  // Triggers are 0..1 (see PadState): clamp directly, NaN -> 0.
  // (A previous (v+1)/2 mapping assumed a -1..1 host range and corrupted
  // already-normalized 0..1 input.)
  raw.l2 = std::isfinite(raw.l2) ? raw.l2 : 0.0f;
  raw.r2 = std::isfinite(raw.r2) ? raw.r2 : 0.0f;
  if (raw.l2 < 0.0f) raw.l2 = 0.0f;
  if (raw.l2 > 1.0f) raw.l2 = 1.0f;
  if (raw.r2 < 0.0f) raw.r2 = 0.0f;
  if (raw.r2 > 1.0f) raw.r2 = 1.0f;
  s.state = raw;
  s.state.connected_users = 1u << uint32_t(slot);
  return Result<void>::Ok();
}

Result<PadState> PadManager::GuestState(int slot) const {
  if (slot < 0 || slot >= kMaxPads)
    return Result<PadState>::Fail(RuntimeError::InvalidArgument, "bad slot");
  if (!slots_[size_t(slot)].connected)
    return Result<PadState>::Fail(RuntimeError::InvalidArgument,
                                  "pad not connected");
  return Result<PadState>::Ok(slots_[size_t(slot)].state);
}

Result<void> PadManager::SetOutput(int slot, PadOutput out) {
  if (slot < 0 || slot >= kMaxPads)
    return Result<void>::Fail(RuntimeError::InvalidArgument, "bad slot");
  if (!slots_[size_t(slot)].connected)
    return Result<void>::Fail(RuntimeError::InvalidArgument,
                              "pad not connected");
  slots_[size_t(slot)].output = out;
  return Result<void>::Ok();
}

Result<PadOutput> PadManager::Output(int slot) const {
  if (slot < 0 || slot >= kMaxPads)
    return Result<PadOutput>::Fail(RuntimeError::InvalidArgument, "bad slot");
  return Result<PadOutput>::Ok(slots_[size_t(slot)].output);
}

}  // namespace kyty::svc
