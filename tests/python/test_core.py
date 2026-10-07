import sys, os, threading, time
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "python"))
from kyty import GuestClock, HandleManager, ObjectType, WaitManager, RuntimeError

def test_clock():
    c = GuestClock()
    t0 = c.monotonic_ns(); g0 = c.guest_ticks()
    c.advance_ns(1000)
    assert c.monotonic_ns() >= t0 + 1000
    assert c.guest_ticks() >= g0 + 1000

def test_handle():
    hm = HandleManager()
    h1 = hm.create(ObjectType.Event); h2 = hm.create(ObjectType.Mutex)
    assert h1.is_ok() and h2.is_ok() and h1.value != h2.value
    assert not hm.lookup(0).is_ok() and not hm.lookup(999999).is_ok()
    hm.destroy(h1.value)
    assert not hm.lookup(h1.value).is_ok() and hm.alive() == 1

def test_wait():
    wm = WaitManager()
    assert wm.register(7).is_ok()
    assert wm.wait(1, 7, 0, 0.01, lambda: True).is_ok()
    threading.Timer(0.02, lambda: wm.wake(7, 1)).start()
    assert wm.wait(2, 7, 10, 0.5, lambda: False).is_ok()
    r = wm.wait(3, 7, 0, 0.02, lambda: False)
    assert not r.is_ok() and r.error == RuntimeError.Timeout
    wm.delete(7)
    assert not wm.wait(4, 7, 0, 0.005, lambda: False).is_ok()
