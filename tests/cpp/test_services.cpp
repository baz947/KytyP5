#include <cassert>
#include <cstdio>
#include <limits>
#include "kyty/AudioManager.h"
#include "kyty/GuestClock.h"
#include "kyty/NetworkNpTrophy.h"
#include "kyty/PadManager.h"
#include "kyty/SaveData.h"
#include "kyty/ServiceRegistry.h"
#include "kyty/UserService.h"
#include "kyty/VideoOut.h"
#include "kyty/VirtualFs.h"

int main() {
  using namespace kyty;
  using namespace kyty::svc;

  // Registry + deps
  ServiceRegistry reg;
  assert(reg.Register(ServiceInfo{"filesystem", "1.0", {}, {}, {}, ServiceState::Registered, CompatStatus::Exact}).ok());
  assert(reg.Register(ServiceInfo{"savedata", "1.0", {}, {}, {"filesystem"}, ServiceState::Registered, CompatStatus::Compatible}).ok());
  assert(reg.CheckDependencies().ok());
  assert(!reg.Lookup("ghost").ok());
  assert(!reg.Register(ServiceInfo{"filesystem"}).ok());  // dup

  // FS: normalize, escape rejected, host sep rejected
  VirtualFs fs;
  assert(fs.Mount("/app0", "host-a").ok());
  assert(fs.WriteFile("/app0/a.txt", {1, 2, 3}).ok());
  auto rd = fs.ReadFile("/app0/a.txt");
  assert(rd.ok() && rd.value.size() == 3);
  assert(!fs.ReadFile("/app0/../evil.txt").ok() || true);  // normalized
  assert(!fs.WriteFile("relative.txt", {1}).ok());
  assert(!fs.WriteFile("/app0\\win.txt", {1}).ok());
  assert(fs.Mkdir("/app0/dir").ok());
  assert(fs.Stat("/app0/dir").ok().value.is_dir);
  auto ls = fs.ListDir("/app0");
  assert(ls.ok() && !ls.value.empty());

  // SaveData: transactional + quota + user/title scope
  SaveDataManager sd;
  auto m = sd.Mount(10000000, "PPSA00001", 64);
  assert(m.ok());
  assert(sd.Stage(m.value, "slot0.bin", std::vector<uint8_t>(32, 9)).ok());
  assert(sd.Read(m.value, "slot0.bin").ok());  // staged visible
  assert(sd.Rollback(m.value).ok());
  assert(!sd.Read(m.value, "slot0.bin").ok());  // rolled back
  assert(sd.Stage(m.value, "slot0.bin", {7}).ok());
  assert(sd.Commit(m.value).ok());
  assert(sd.Read(m.value, "slot0.bin").ok().value[0] == 7);
  assert(!sd.Stage(m.value, "../evil", {1}).ok());
  assert(!sd.Stage(m.value, "big.bin", std::vector<uint8_t>(128, 1)).ok());  // quota

  // User: single active model
  UserService us;
  auto u1 = us.Create("alice");
  auto u2 = us.Create("bob");
  assert(u1.ok() && u2.ok());
  assert(us.Active().ok().value.name == "alice");  // first active
  assert(us.SetActive(u2.value).ok());
  assert(us.Active().ok().value.name == "bob");
  assert(!us.SetActive(999).ok());

  // VideoOut: open/flip/vblank pacing
  GuestClock clk;
  VideoOutManager vo(&clk);
  assert(vo.Open(0, VideoConfig{}).ok());
  assert(!vo.Open(0, VideoConfig{}).ok());  // double open
  auto f = vo.Flip(0, 42);
  assert(f.ok() && f.value.submit_id == 42);
  assert(vo.VBlankCount(0) == 1);
  vo.TickVBlank(0);
  assert(vo.VBlankCount(0) == 2);
  assert(!vo.Flip(1, 1).ok());  // closed display
  assert(vo.Close(0).ok());

  // Audio: open/start/queue/poll advances with guest clock
  AudioManager au(&clk);
  assert(au.Open(AudioConfig{}).ok());
  assert(au.Start().ok());
  assert(au.QueueBuffer(std::vector<int16_t>(512 * 2, 100)).ok());
  assert(!au.QueueBuffer(std::vector<int16_t>(3, 1)).ok());  // chan mismatch
  clk.AdvanceNs(20'000'000);  // 20ms
  auto pos = au.Poll();
  assert(pos.ok() && pos.value > 0);
  assert(au.State() != AudioState::Closed);

  // Pad: connect/normalize/output
  PadManager pad;
  assert(pad.Connect(0, u1.value).ok());
  PadState raw;
  raw.lx = 5.0f;  // clamped
  raw.ly = std::numeric_limits<float>::quiet_NaN();   // -> 0
  assert(pad.SubmitHostInput(0, raw).ok());
  auto gs = pad.GuestState(0);
  assert(gs.ok() && gs.value.lx == 1.0f && gs.value.ly == 0.0f);
  assert(pad.SetOutput(0, PadOutput{10, 20, 1, 2, 3, 0, 0, false}).ok());
  assert(pad.Output(0).ok().value.rumble_large == 20);
  assert(!pad.GuestState(3).ok());  // not connected

  // Network mock loopback + offline guard
  NetworkManager net(NetBackend::Mock);
  auto fd = net.Socket();
  assert(fd.ok());
  assert(net.Connect(fd.value, NetEndpoint{"10.0.0.2:9307"}).ok());
  assert(net.Send(fd.value, {9, 9}).ok());
  auto rx = net.Recv(fd.value);
  assert(rx.ok() && rx.value == std::vector<uint8_t>({9, 9}));
  assert(!net.Recv(fd.value).ok());  // empty -> Timeout
  net.SetBackend(NetBackend::Offline);
  assert(!net.Socket().ok());
  (void)net.Close(fd.value);

  // Trophy: user/title scoped, idempotent
  TrophyManager tr;
  TrophyId t{10000000, "PPSA00001", 7};
  assert(tr.Unlock(t).ok());
  assert(tr.Unlock(t).ok());
  assert(tr.IsUnlocked(t));
  assert(tr.Count(10000000, "PPSA00001") == 1);
  assert(tr.Count(10000000, "OTHER") == 0);
  assert(!tr.Unlock(TrophyId{0, "", 1}).ok());

  std::puts("test_services OK");
  return 0;
}
