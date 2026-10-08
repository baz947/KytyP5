"""Runner without pytest (this machine has no pytest / no C++ toolchain)."""
import os, sys, traceback
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "python"))

import tests.python.test_memory as tm
import tests.python.test_fault as tf
import tests.python.test_core as tc
import tests.python.test_linker as tl
import tests.python.test_services as ts
import tests.python.test_gpu as tg
import tests.python.test_shader as tsh
import tests.python.test_infra as ti
import tests.python.test_isa_corpus as tic
import tests.python.test_backend_bridge as tbb
import tests.python.test_p2_rdna as tp2
import tests.python.test_diff_upstream as tdu
import tests.python.test_soak as tso
import tests.python.test_spirv as tsp
import tests.python.test_patches as tpa

mods = [tm, tf, tc, tl, ts, tg, tsh, ti, tic, tbb, tp2, tdu, tso, tsp, tpa]
failed = 0
total = 0
for m in mods:
    for f in sorted(dir(m)):
        if f.startswith("test_"):
            total += 1
            try:
                getattr(m, f)()
                print(f"PASS {m.__name__}::{f}")
            except Exception:
                failed += 1
                print(f"FAIL {m.__name__}::{f}")
                traceback.print_exc()
print(f"\n{total-failed}/{total} passed")
sys.exit(1 if failed else 0)
