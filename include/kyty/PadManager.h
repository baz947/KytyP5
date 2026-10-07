#pragma once
// Controller (Pad/HID) — README §10.
// Host HID -> Input Normalizer -> Guest Controller;
// Guest -> rumble/lightbar/triggers/speaker -> Host HID.
#include <array>
#include <cstdint>
#include "kyty/Compat.h"

namespace kyty::svc {

struct PadButtons {
  bool cross = false, circle = false, square = false, triangle = false;
  bool l1 = false, r1 = false, l3 = false, r3 = false;
  bool options = false, share = false, ps = false;
  bool dpad_up = false, dpad_down = false, dpad_left = false, dpad_right = false;
};

struct PadState {
  PadButtons buttons;
  float lx = 0, ly = 0, rx = 0, ry = 0;  // -1..1 normalized
  float l2 = 0, r2 = 0;                  // 0..1 triggers
  uint32_t connected_users = 0;          // bitmask of user slots
};

struct PadOutput {
  uint8_t rumble_small = 0, rumble_large = 0;
  uint8_t light_r = 0, light_g = 0, light_b = 0;
  uint8_t trigger_l = 0, trigger_r = 0;  // adaptive trigger effect 0..255
  bool speaker_mute = false;
};

class PadManager {
 public:
  static constexpr int kMaxPads = 4;
  Result<void> Connect(int slot, uint32_t user_id);
  Result<void> Disconnect(int slot);
  // Normalized host input in [-1,1]/[0,1]; clamps out-of-range (no NaN leak).
  Result<void> SubmitHostInput(int slot, PadState raw);
  Result<PadState> GuestState(int slot) const;
  Result<void> SetOutput(int slot, PadOutput out);
  Result<PadOutput> Output(int slot) const;

 private:
  static float ClampAxis(float v);
  struct Slot {
    bool connected = false;
    uint32_t user_id = 0;
    PadState state;
    PadOutput output;
  };
  std::array<Slot, kMaxPads> slots_;
};

}  // namespace kyty::svc
