import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "python"))
from kyty import RuntimeError
from kyty.gpu_semantic import *

def test_format_and_image():
    assert resolve_format(Format.RGBA8) == FormatResolution.Native
    assert resolve_format(Format.BC1) == FormatResolution.Emulated
    assert resolve_format(Format.BC1, True) == FormatResolution.Native
    assert resolve_format(Format.Unknown) == FormatResolution.Unsupported
    assert make_image(0x10000, Format.RGBA8, 256, 4, 1, 7).is_ok()
    assert not make_image(0, Format.RGBA8, 256).is_ok()
    assert not make_image(0x1000, Format.Unknown, 4).is_ok()

def test_materialize_and_tracking():
    img = make_image(0x10000, Format.RGBA8, 64).value
    assert materialize(img, FormatResolution.Native)[0] == MaterializeResult.Success
    bc = make_image(0x10000, Format.BC1, 64).value
    assert materialize(bc, FormatResolution.Emulated)[0] == MaterializeResult.NeedFallback
    assert materialize({}, FormatResolution.Native)[0] == MaterializeResult.InvalidDescriptor
    assert track_runtime(0xBDA0, True).is_ok()
    r = track_runtime(0, False)
    assert not r.is_ok() and "valid runtime value" in r.detail

def test_wave_barrier_caps_timeline_hang():
    assert select_wave_backend(False, False, False).is_ok()
    assert select_wave_backend(True, False, True).value == WaveBackend.Split
    assert not select_wave_backend(True, False, False).is_ok()
    assert validate_barrier(0xFFFFFFFF, 32).is_ok()
    r = validate_barrier(0x1, 32)
    assert not r.is_ok() and r.error == RuntimeError.ShaderFailure
    assert resolve_req("descriptors", {"desc_arrays": True}) == "Native"
    assert resolve_req("ray", {}) == "Unavailable"
    assert resolve_req("wave64", {"sub64": False}) == "Lowered"
    tl = GpuTimeline()
    h = tl.submit(10, "resA").value
    assert not tl.can_destroy("resA")
    tl.complete(h)
    assert tl.can_destroy("resA")
    assert not observe_hang(10).is_ok()
    assert observe_hang(1_000_001).is_ok()
