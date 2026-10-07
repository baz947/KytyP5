# KytyPS5 --- Deep Compatibility & Architecture Review

**Date:** 2026-10-07\
**Scope:** CPU/ABI, RuntimeLinker, Kernel, Memory, GPU, Shader
Recompiler, Resources, Services, Synchronization, Diagnostics and
Testing.

## Executive Summary

KytyPS5 should be treated as a **PS5 semantic compatibility runtime**,
not merely a `PS5 -> Vulkan` emulator.

``` text
PS5 Guest
  -> Guest ABI
  -> RuntimeLinker / Modules
  -> Kernel + Services
  -> Guest Memory + Fault Manager
  -> CPU/JIT + GPU Semantic Core
  -> Shader/Resource/Image model
  -> Host Capability Layer
  -> Vulkan / Host OS
```

The main long-term problem is not simply missing APIs. It is keeping one
coherent semantic model across CPU, ABI, kernel, memory, runtime
services, GPU resources, shaders, Vulkan and presentation.

Recent public issues demonstrate failures in shader resource tracking,
MIMG/CFG coverage, resource materialization, runtime-linker paths,
unresolved imports, Vulkan/device behavior and memory/texture
synchronization. The current test suite already covers many focused
areas such as resource tracking/materialization, page management,
synchronization, audio timing and controller haptics.

------------------------------------------------------------------------

## 1. Global Compatibility Contract

Every function, resource feature and host capability should have an
explicit status:

``` text
Exact | Compatible | Emulated | Partial | SafeStub | Unsupported | WrongABI | Broken
```

GPU capability resolution:

``` text
Native | Lowered | Emulated | Fallback | Unavailable
```

Runtime errors:

``` text
InvalidArgument
InvalidGuestMemory
MissingDependency
UnresolvedImport
WrongABI
UnsupportedFeature
InvalidResource
ShaderFailure
HostMemoryFailure
HostGpuFailure
Timeout
Deadlock
InternalInvariant
```

Never use `return 0` as a universal unsupported-function policy.

------------------------------------------------------------------------

## 2. Guest Memory --- P0

Separate:

``` text
GuestAddress
GuestPage
HostMapping
GpuAddress
GpuResource
```

Recommended API:

``` cpp
MemoryTranslation Translate(GuestAddress, size, Access);
Read(...); Write(...); CopyFromGuest(...); CopyToGuest(...);
Map(...); Unmap(...); Protect(...); Synchronize(...);
```

Page states:

``` text
Unmapped Reserved Committed ReadOnly ReadWrite Executable
GpuVisible Faulting Evicted
```

Track CPU, GPU, descriptor, resource and mapping versions. Cross-page
operations must be page-aware.

### BDA fault path

``` text
Shader BDA -> page table -> access
                    |
                    +-> fault -> resolve mapping -> update -> bounded retry
```

Persistent faults become controlled failures; never infinite retry.

------------------------------------------------------------------------

## 3. Kernel / Scheduler --- P0

Host condition variables are blocking mechanisms, not the guest
scheduler.

``` text
Guest Sync Object
 -> Kernel Wait Manager
 -> Guest Scheduler State
 -> Host Blocking Primitive
```

Guest thread states:

``` text
Created Ready Running Sleeping Waiting Suspended Signaled Terminated
```

Unified waiting:

``` text
Wait(thread, object, condition, timeout)
Wake(object, count)
Cancel(thread)
Timeout(thread)
Delete(object)
```

Lost-wakeup-safe sequence:

``` text
compare -> register waiter atomically -> recheck -> sleep
```

Use priority-aware queues, priority inheritance for mutexes, centralized
cancellation and object lifetime rules.

------------------------------------------------------------------------

## 4. Guest Clock --- P0

Create one authoritative `GuestClock` for:

``` text
real time
monotonic time
process time
thread time
guest ticks
```

Kernel, GPU, Audio, VideoOut, Network, Input and timers must share the
same guest timing model.

------------------------------------------------------------------------

## 5. RuntimeLinker --- P0

Decompose the current large responsibility set into:

``` text
ElfLoader
DynamicParser
ModuleManager
DependencyResolver
SymbolResolver
RelocationEngine
TlsManager
AbiAdapter
GuestExecutionContext
FaultBoundary
```

Module lifecycle:

``` text
Discovered -> Loading -> Mapped -> Parsed -> DependenciesResolved
-> Relocated -> Initialized -> Running -> Stopping -> Unloaded / Failed
```

A `GuestModule` should own ELF image, memory map, dynamic metadata,
symbols, TLS and relocation state.

------------------------------------------------------------------------

## 6. Symbol Resolution --- P0

Do not use only `name -> address`.

``` cpp
struct SymbolKey {
    LibraryId library;
    string name;
    SymbolType type;
    Version version;
};
```

Resolution must consider module, library, type, version, binding,
visibility and ABI.

Function imports and object imports must be separate paths. TLS objects,
function pointers, vtables and runtime structures must not be treated as
generic integers.

------------------------------------------------------------------------

## 7. ABI --- P0

Validate:

``` text
calling convention
register/stack arguments
argument width
alignment
struct packing
return values
FP/vector arguments
pointer representation
TLS access
variadic calls
```

Required flow:

``` text
Guest Call -> ABI Decode -> Validated Arguments -> Implementation
           -> ABI Encode -> Guest Return
```

Guest pointers must pass through GuestMemory validation rather than
arbitrary host casts.

------------------------------------------------------------------------

## 8. Relocations / PLT / Thunks --- P0

Common x86-64 relocations already form a base, but relocation must
become an independent engine returning:

``` text
Applied Deferred Stubbed Unsupported Invalid
```

Deferred relocation is essential when a dependency is not yet loaded.

Function call paths should be explicit:

``` text
resolved direct address
or generated thunk
or lazy resolver
or semantic stub
```

A thunk should carry symbol, module, ABI, target and failure policy.

------------------------------------------------------------------------

## 9. TLS --- P0

Separate Guest TLS from Host TLS.

``` cpp
struct GuestTls {
    Tcb tcb;
    TlsBlock modules[];
    uint64_t errno_addr;
    uint64_t locale_addr;
    uint64_t runtime_state;
};
```

Each guest thread must have correct TLS. Platform-specific `fs:`
rewriting should live behind `GuestAbiAdapter`, not be scattered through
the loader.

Conceptual operations:

``` text
GUEST_TLS_GET
GUEST_TLS_SET
GUEST_TCB_GET
```

------------------------------------------------------------------------

## 10. Runtime Services --- P0/P1

Create a `ServiceRegistry` containing:

``` text
libkernel
libc
Filesystem
SaveData
UserService
VideoOut
Audio
Pad/HID
Network
NP
Trophy
System Parameters
```

Each service exposes version, functions, capabilities, dependencies,
state and diagnostics.

### Filesystem

``` text
Guest FS -> Virtual FS -> Host Backend
```

Host paths and host error codes must not leak into guest semantics.

### SaveData

``` text
Guest SaveData API
 -> SaveData Manager
 -> Virtual SaveData FS
 -> Host Storage Backend
```

Model user, title ID, mount, files, transactions, metadata, quota and
commit.

### User

One `GuestUser` model must consistently feed Thread/TLS, SaveData,
Trophy, Controller and NP.

### VideoOut

Treat it as a state machine:

``` text
display/output/surface/flip/VBlank/VRR/frame queue/HDR/timing
```

Recommended:

``` text
Guest VideoOut -> VideoOut Manager -> Frame Scheduler -> GPU Timeline -> Host Present
```

### Audio

Use a shared Audio/Guest clock, buffer queue, sample position, latency
and device state.

### Controller

``` text
Host HID -> Input Normalizer -> Guest Controller
Guest -> rumble/lightbar/adaptive trigger/speaker -> Host HID
```

### Network / NP / Trophy

Use translated sockets and errors. For NP, prefer a deterministic
compatibility backend (`Offline`, `Local`, `LAN`, `Mock`) rather than
attempting to reproduce proprietary Sony backend infrastructure. Trophy
should be transactional and user/title aware.

------------------------------------------------------------------------

## 11. GPU Semantic Core --- P0

Vulkan must not be the semantic source of truth.

``` text
PS5 GPU commands
 -> Guest GPU Semantic Core
 -> Canonical Resources
 -> Host Capability Resolver
 -> Vulkan
```

Track guest submit ID, host submit ID, queue, command range, shaders,
resources and timeline dependencies.

------------------------------------------------------------------------

## 12. Shader Recompiler --- P0

The existing pipeline is broadly:

``` text
Decode
 -> CFG
 -> Translate
 -> IR
 -> SSA/control-flow simplification
 -> DCE/read-lane/tessellation lowering
 -> Resource Tracking
 -> Resource Specialization/Materialization
 -> Shader Info
 -> Binding Allocation
 -> SPIR-V
```

Critical rule:

``` text
Decoder coverage != CFG coverage != Translator coverage != IR coverage != Backend coverage
```

Use an instruction capability registry with status per stage.

------------------------------------------------------------------------

## 13. MIMG / Image Semantics --- P0

Use semantic metadata rather than one implementation per raw encoding:

``` cpp
ImageSampleInfo {
  dimension;
  arrayed;
  multisampled;
  compare;
  lod_mode;
  derivative_mode;
  has_offset;
  has_clamp;
}
```

MIMG decoding, CFG support, translation and SPIR-V emission must be
independently testable.

------------------------------------------------------------------------

## 14. Wave32 / Wave64 --- P0

Represent explicitly:

``` text
EXEC
VCC
SCC
lane ID
scalar state
cross-lane state
```

IR should expose:

``` text
READ_LANE WRITE_LANE BROADCAST SHUFFLE BALLOT ACTIVE_MASK
```

Backend modes:

``` text
Native Split Software
```

Wave64 must not silently degrade into incorrect Wave32 semantics.

------------------------------------------------------------------------

## 15. Resource Tracking --- P0

Use explicit resolution states:

``` text
Static Derived Dynamic Runtime Unknown Invalid
```

``` cpp
ResourceHandle {
    GuestAddress address;
    DescriptorIndex descriptor;
    ResourceResolution resolution;
};
```

Resource expression equality should be semantic, not merely structural.
Cache resolution using value + CFG block + memory version + wave
state/path condition where required.

Recent Demon's Souls reports show resource-tracking failure at
`GetImageResource ... not a valid runtime value`; this is exactly the
class of problem the canonical model must solve rather than patch per
title.

------------------------------------------------------------------------

## 16. Resource Materialization --- P0

Never reduce materialization to boolean fatal/non-fatal.

Return:

``` text
Success
NeedFallback
NeedShaderRewrite
NeedViewReinterpretation
Unsupported
InvalidDescriptor
InvalidMemory
```

Recent Astro's Playroom reports show `MaterializeResources()` reaching a
fatal assertion entering CPU Plaza; this should become a structured
resource failure with recovery options.

------------------------------------------------------------------------

## 17. Canonical Resource / Image Model --- P0

A canonical image should contain:

``` text
address
format
dimension
width/height/depth
mip count
array layers
base mip/range
base layer/range
samples
aspect
swizzle
usage
FMASK/HTILE/depth/stencil flags
resolution state
```

One guest allocation may represent Buffer, Image, Storage or BDA;
support aliasing explicitly.

------------------------------------------------------------------------

## 18. Formats / Views / Descriptors --- P0

Format resolver:

``` text
Native
CompatibleView
Reinterpreted
Emulated
Unsupported
```

Centralize validation of:

``` text
format
aspect
mip range
layer range
view type
sample count
usage
```

Dynamic descriptor path:

``` text
shader -> guest descriptor address -> decode -> canonical resource
       -> host capability -> materialized view
```

Uniformity must be tracked as:

``` text
Uniform / Divergent / Unknown
```

because non-uniform descriptor indexing depends on it.

------------------------------------------------------------------------

## 19. Atomics / Barriers / Memory Semantics --- P0

Represent atomics with opcode, width, type, scope, semantics and target.

Targets:

``` text
Buffer Image LDS/Shared Global BDA
```

Scopes:

``` text
Invocation Subgroup Workgroup Device System
```

Semantics:

``` text
Relaxed Acquire Release AcquireRelease SequentiallyConsistent
```

Barriers must encode execution scope, memory scope, semantics and
resource visibility. Detect divergent barriers.

------------------------------------------------------------------------

## 20. Persistent Shader Loops --- P0

For loop-cap/hang diagnostics collect:

``` text
shader hash
stage
PC
wave size
EXEC
loop header
iteration count
atomics
memory dependencies
resource IDs
changed/unchanged values
```

This turns an apparent GPU hang into a reproducible semantic failure.

------------------------------------------------------------------------

## 21. Host GPU Capability Layer --- P0

Do not equate `Vulkan 1.3` with one universal hardware feature set.

Track:

``` text
runtime descriptor arrays
non-uniform indexing
update-after-bind
partially-bound
BDA
ray query/AS
image atomics
integer64
float16
subgroup32
subgroup64
```

Resolve each PS5 requirement to:

``` text
Native / Lowered / Emulated / Fallback / Unavailable
```

------------------------------------------------------------------------

## 22. GPU Timeline / Scheduler / Lifetime --- P0

Use:

``` text
Guest Timeline -> Master Timeline -> Vulkan Timeline Semaphore
```

Resource lifetime must obey GPU completion, not just guest handle
release.

Buffer/image cache keys should include guest address, range, memory
version, descriptor fingerprint, format, dimensions, mips, layers and
usage.

Pipeline cache keys should include shader hash, IR version, resource
model version, host capability hash, descriptor layout, format
capability, wave mode and backend version.

------------------------------------------------------------------------

## 23. Fault Manager / Deadlock Detector --- P0

Every serious fault becomes:

``` cpp
FaultRecord {
  kind;
  guest_address;
  size;
  access;
  thread;
  guest_pc;
  queue;
  submit;
  resource;
  shader;
  action;
  retry_count;
}
```

Fault storm metrics:

``` text
faults/sec
unique addresses
same-page count
retry rate
resolution success
CPU/GPU progress
```

Unified dependency graph nodes:

``` text
GuestThread GpuQueue Resource Timeline KernelObject
```

Edges:

``` text
Wait Own Read Write Signal Dependency
```

Classify:

``` text
CPU_DEADLOCK
GPU_DEADLOCK
CPU_GPU_DEADLOCK
MEMORY_STALL
RESOURCE_STALL
SHADER_HANG
HOST_GPU_FAILURE
UNKNOWN
```

------------------------------------------------------------------------

## 24. Compatibility Database --- P1

Use feature requirements, not only game names:

``` yaml
title_id: PPSAxxxxx
version: 01.xxx
status: ingame
requirements:
  wave64: true
  dynamic_descriptors: true
  ray_tracing: false
known_issues:
  - subsystem: resource_tracking
    status: broken
```

A game becomes evidence for a semantic requirement; fixes should benefit
every game sharing that requirement.

------------------------------------------------------------------------

## 25. Testing Strategy

### Unit tests

Memory, page translation, protection, versioning, BDA, aliasing, waits,
timeouts, cancellation, handles, ELF, symbols, relocations, TLS, ABI,
descriptors, formats, mip/layers, wave behavior, atomics, barriers,
resource tracking and materialization.

### Differential tests

``` text
Guest input -> expected semantic result -> Kyty result -> diff
```

### Fuzzing

Fuzz ELF, dynamic sections, symbols, relocations, descriptors, formats,
shaders, CFG, IR, guest pointers, handles and wait primitives.

Every crash becomes a reproducible seed and regression test.

### Chaos mode

Inject deterministic:

``` text
GPU delay
memory fault
descriptor miss
thread delay
timeout
device lost
host allocation failure
```

### Game corpus

Organize by feature:

``` text
wave64/
dynamic_descriptors/
image_formats/
depth_stencil/
fmask/
htile/
bda/
atomics/
barriers/
ray_tracing/
videoout/
savedata/
tls/
network/
```

------------------------------------------------------------------------

## 26. Diagnostics / Cross-Subsystem Trace

Every important operation should share a correlation identity:

``` text
Frame
Thread
Runtime Call
GPU Submit
Shader
Resource
Guest Address
GPU Timeline
```

A failure should be reconstructable from:

``` text
Guest PC
module/symbol
ABI
thread/TLS
memory range
resource fingerprint
shader hash
queue/submit
host result
recovery action
```

------------------------------------------------------------------------

## 27. Compatibility Score

Do not define compatibility as `boots = yes/no`.

Track:

``` text
Boot
Menu
Gameplay
Rendering
Audio
Input
Save
Loading
Long-run stability
Performance
```

This makes regressions measurable.

------------------------------------------------------------------------

## 28. Implementation Order

### P0

``` text
1  Guest Memory + Translation
2  Fault Manager
3  Guest Thread / Wait Manager
4  Guest Clock
5  Handle/Object Manager
6  RuntimeLinker decomposition
7  Symbol Resolver
8  Relocation Engine
9  TLS Manager
10 ABI Adapter
11 Guest Execution Boundary
12 Service Registry
13 Filesystem
14 SaveData
15 User Service
16 VideoOut
17 Audio
18 Controller
19 Network/NP/Trophy
20 Canonical Resource Model
21 Descriptor Decoder
22 Format Resolver
23 Resource Tracking
24 Resource Materialization
25 Wave32/Wave64
26 Atomic/Barrier Model
27 Host GPU Capability Layer
28 GPU Timeline/Scheduler
29 Buffer/Image Lifetime
30 Pipeline Cache
```

### P1

``` text
Compatibility Database
Differential Tests
Fuzzing
Chaos Tests
Cross-platform CI
Feature-oriented game corpus
```

### P2

``` text
Performance optimization
Advanced online services
Advanced presentation
Long-run optimization/stability
```

------------------------------------------------------------------------

## 29. Definition of Done

A subsystem is complete only when:

``` text
1. Semantic model exists
2. ABI/API boundary is explicit
3. Host implementation is isolated
4. Error model exists
5. Recovery policy exists
6. Diagnostics exist
7. Unit tests exist
8. Regression test exists
9. Cross-platform behavior is defined
10. Ownership/lifetime is defined
```

A game feature is complete only when decode/translate, runtime, memory,
resources, GPU and presentation agree semantically.

------------------------------------------------------------------------

## 30. Final Architecture

``` text
                         PS5 GAME
                             |
                             v
                    +----------------+
                    |   Guest ABI    |
                    +-------+--------+
                            |
                    +-------v--------+
                    | RuntimeLinker  |
                    +-------+--------+
                            |
             +--------------+--------------+
             |                             |
             v                             v
      +-------------+               +-------------+
      | Guest Kernel|               |  Services   |
      +------+------+               +------+------+
             |                             |
             +--------------+--------------+
                            |
                    +-------v--------+
                    | Guest Memory   |
                    | + Fault Mgr   |
                    +-------+--------+
                            |
              +-------------+-------------+
              |                           |
              v                           v
       +-------------+             +-------------+
       | CPU / JIT   |             | GPU Semantic|
       +-------------+             | Core        |
                                   +------+------+
                                          |
                    +---------------------+---------------------+
                    |                     |                     |
                    v                     v                     v
                 Shader              Resources              Scheduler
                    |                     |                     |
                    +---------------------+---------------------+
                                          |
                                  +-------v-------+
                                  | Host Caps     |
                                  +-------+-------+
                                          |
                                      Vulkan
                                          |
                                      Host GPU
```

------------------------------------------------------------------------

## 31. Final Engineering Rule

Every new bug must be processed as:

``` text
Crash
 -> classify
 -> identify violated PS5 semantic contract
 -> identify common subsystem
 -> implement common behavior
 -> add regression test
 -> update compatibility database
 -> retest affected feature corpus
```

Never make the default fix:

``` text
if (game == X) special_patch();
```

unless the behavior is genuinely title/version-specific.

The target is not:

> "Can this game boot?"

The target is:

> **"Which PS5 semantic requirement is unsupported, and can we implement
> that requirement once so every game using it benefits?"**

------------------------------------------------------------------------

## References / Evidence

-   KytyPS5 repository: https://github.com/KytyPS5/KytyPS5
-   KytyPS5 issues: https://github.com/KytyPS5/KytyPS5/issues
-   ShaderRecompiler.cpp:
    https://github.com/KytyPS5/KytyPS5/blob/main/src/graphics/shader/recompiler/ShaderRecompiler.cpp
-   CMake tests:
    https://github.com/KytyPS5/KytyPS5/blob/main/CMakeLists.txt
-   Recent resource-tracking issue #855:
    https://github.com/KytyPS5/KytyPS5/issues/855
-   Recent shader opcode aggregate issue #281:
    https://github.com/KytyPS5/KytyPS5/issues/281
-   Recent Astro's Playroom resource-materialization issue #1005:
    https://github.com/KytyPS5/KytyPS5/issues/1005
-   Recent Astro Bot Linux regression #1070:
    https://github.com/KytyPS5/KytyPS5/issues/1070
-   Tour de France 2023 issue #485:
    https://github.com/KytyPS5/KytyPS5/issues/485
-   Teardown texture-cache issue #226:
    https://github.com/KytyPS5/KytyPS5/issues/226

## Conclusion

KytyPS5 already contains the major building blocks. The highest-value
work is to make them **semantic, explicit, versioned, testable and
recoverable**. If the P0 architecture is implemented, future
compatibility work becomes additive rather than a growing collection of
title-specific patches.
