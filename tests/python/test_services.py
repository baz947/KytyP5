import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "python"))
from kyty import RuntimeError
from kyty.service_registry import ServiceRegistry
from kyty.virtual_fs import VirtualFs
from kyty.save_data import SaveDataManager
from kyty.user_service import UserService
from kyty.services import (VideoOutManager, AudioManager, PadManager,
                           NetworkManager, TrophyManager)
from kyty.guest_clock import GuestClock

def test_registry():
    r = ServiceRegistry()
    assert r.register("filesystem").is_ok()
    assert r.register("savedata", ["filesystem"]).is_ok()
    assert r.check().is_ok()
    assert not r.lookup("ghost").is_ok()
    assert not r.register("filesystem").is_ok()

def test_fs():
    fs = VirtualFs()
    assert fs.write("/app0/a.txt", b"\x01\x02\x03").is_ok()
    assert fs.read("/app0/a.txt").value == b"\x01\x02\x03"
    assert not fs.write("relative.txt", b"x").is_ok()
    assert not fs.write("/app0\\win.txt", b"x").is_ok()
    assert fs.mkdir("/app0/dir").is_ok()
    assert fs.stat("/app0/dir").value["dir"]
    assert fs.listdir("/app0").value

def test_savedata():
    sd = SaveDataManager()
    m = sd.mount(10000000, "PPSA00001", 64).value
    assert sd.stage(m, "slot0.bin", b"\x09" * 32).is_ok()
    assert sd.read(m, "slot0.bin").is_ok()
    sd.rollback(m)
    assert not sd.read(m, "slot0.bin").is_ok()
    sd.stage(m, "slot0.bin", b"\x07")
    sd.commit(m)
    assert sd.read(m, "slot0.bin").value == b"\x07"
    assert not sd.stage(m, "../evil", b"x").is_ok()
    assert not sd.stage(m, "big.bin", b"\x01" * 128).is_ok()

def test_user():
    us = UserService()
    a = us.create("alice").value; b = us.create("bob").value
    assert us.active_user().value[1] == "alice"
    us.set_active(b)
    assert us.active_user().value[1] == "bob"
    assert not us.set_active(999).is_ok()

def test_video_audio_pad_net_trophy():
    clk = GuestClock()
    vo = VideoOutManager(clk)
    assert vo.open(0).is_ok() and not vo.open(0).is_ok()
    f = vo.flip(0, 42)
    assert f.is_ok() and f.value["submit"] == 42 and vo.vblank(0) == 1
    assert not vo.flip(1, 1).is_ok()
    au = AudioManager(clk)
    assert au.open().is_ok() and au.start().is_ok()
    assert au.queue([100] * 1024).is_ok()
    assert not au.queue([1, 2, 3]).is_ok()
    clk.advance_ns(20_000_000)
    assert au.poll().value > 0
    pad = PadManager()
    assert pad.connect(0, 10000000).is_ok()
    assert pad.submit(0, lx=5.0, ly=float("nan")).is_ok()
    st = pad.state(0).value
    assert st["lx"] == 1.0 and st["ly"] == 0.0
    # triggers are 0..1 already: 0.25 must survive, NaN->0, 5.0->1.0
    assert pad.submit(0, l2=0.25, r2=0.75).is_ok()
    st = pad.state(0).value
    assert st["l2"] == 0.25 and st["r2"] == 0.75
    assert pad.submit(0, l2=float("nan"), r2=5.0).is_ok()
    st = pad.state(0).value
    assert st["l2"] == 0.0 and st["r2"] == 1.0
    assert not pad.state(3).is_ok()
    net = NetworkManager()
    fd = net.socket().value
    net.send(fd, b"\x09\x09")
    assert net.recv(fd).value == b"\x09\x09"
    assert not net.recv(fd).is_ok()
    net2 = NetworkManager(offline=True)
    assert not net2.socket().is_ok()
    tr = TrophyManager()
    assert tr.unlock(10000000, "PPSA00001", 7).is_ok()
    assert tr.count(10000000, "PPSA00001") == 1
    assert not tr.unlock(0, "", 1).is_ok()
