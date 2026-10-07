"""Backend lowering + corpus<->compat bridge mirror."""


def lower(info_uses_wave64, info_uses_cmp, host_sub64):
    if info_uses_wave64 and not host_sub64:
        return ("Wave64Split", ["wave64.lo", "wave64.hi"], "w32x2")
    if info_uses_cmp:
        return ("MimgCompare", ["mimg.compare"], "native")
    return ("None", ["native"], "native")


SUBSYSTEM_FOR = {
    "wave64": "wave_model", "dynamic_descriptors": "resource_tracking",
    "image_formats": "format_resolver", "depth_stencil": "format_resolver",
    "bda": "guest_memory", "atomics": "atomic_model",
    "barriers": "barrier_model", "videoout": "videoout",
    "savedata": "savedata", "tls": "tls_manager", "network": "network",
    "ray_tracing": "host_caps",
}


def file_failures(db, tid, ver, results, corpus):
    key = (tid, ver)
    if key not in db:
        return None
    n = 0
    for name, status, actual, exp in results:
        if status != "fail":
            continue
        feat = next((f for f, nm, _, _, _ in corpus if nm == name), "?")
        db[key].append((SUBSYSTEM_FOR.get(feat, "unknown"), actual))
        n += 1
    return n
