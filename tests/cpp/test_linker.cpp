#include <cassert>
#include <cstdio>
#include "kyty/AbiAdapter.h"
#include "kyty/ElfLoader.h"
#include "kyty/ExecutionContext.h"
#include "kyty/GuestMemory.h"
#include "kyty/ModuleManager.h"
#include "kyty/RelocationEngine.h"
#include "kyty/SymbolResolver.h"
#include "kyty/TlsManager.h"

int main() {
  using namespace kyty;
  using namespace kyty::linker;

  // 1. ELF parse: valid test image + bad magic.
  auto img = ElfLoader::MakeTestImage({"libkernel.sprx"}, "libtest.sprx", true);
  auto pe = ElfLoader::ParseImage(img);
  assert(pe.ok());
  assert(pe.value.machine == kMachineX86_64);
  assert(pe.value.tls.present);
  assert(!pe.value.needed.empty());
  std::vector<uint8_t> bad = {0x7F, 'E', 'L', 'X', 0, 0};
  assert(!ElfLoader::ParseImage(bad).ok());

  // 2. Lifecycle: linear pipeline enforced, skip rejected.
  ModuleManager mm;
  auto a = mm.Discover("libkernel.sprx");
  auto b = mm.Discover("libtest.sprx");
  assert(a.ok() && b.ok());
  assert(mm.SetState(a.value, ModuleState::Loading).ok());
  assert(mm.SetState(a.value, ModuleState::Mapped).ok());
  assert(!mm.SetState(a.value, ModuleState::Running).ok());  // skip -> fail
  assert(mm.SetState(a.value, ModuleState::Parsed).ok());

  // 3. Dependencies: order + missing + cycle.
  ModuleManager m2;
  auto lk = m2.Discover("libkernel.sprx").value;
  auto ap = m2.Discover("app.sprx").value;
  assert(m2.AddDependency(ap, "libkernel.sprx").ok());
  auto order = m2.ResolveOrder();
  assert(order.ok() && order.value.front() == lk && order.value.back() == ap);
  ModuleManager m3;
  auto x = m3.Discover("x.sprx").value;
  assert(m3.AddDependency(x, "ghost.sprx").ok());
  assert(!m3.ResolveOrder().ok());

  // 4. Symbols: func vs object separation + version + WrongABI.
  SymbolResolver sr;
  assert(sr.Define(SymbolEntry{{1, "puts", SymbolType::Func, "1.0"}, 1,
                               0x1000, 16, SymbolBinding::Global, true})
             .ok());
  assert(sr.Define(SymbolEntry{{1, "errno", SymbolType::Object, ""}, 1, 0x2000,
                               4, SymbolBinding::Global, true})
             .ok());
  assert(sr.ResolveFunction(1, "puts", "1.0", 2).ok());
  assert(!sr.ResolveFunction(1, "puts", "2.0", 2).ok());  // version mismatch
  assert(sr.ResolveObject(1, "errno", "", 2).ok());
  auto w = sr.Resolve(SymbolKey{1, "puts", SymbolType::Object, "1.0"}, 2);
  assert(!w.ok() && w.error == RuntimeError::WrongABI);  // func as object
  auto u = sr.ResolveFunction(1, "nope", "", 2);
  assert(!u.ok() && u.error == RuntimeError::UnresolvedImport);
  // TLS separate path
  assert(sr.Define(SymbolEntry{{1, "tlsvar", SymbolType::Tls, ""}, 1, 0, 8,
                               SymbolBinding::Global, true})
             .ok());
  assert(sr.ResolveTls(1, "tlsvar", "", 2).ok());
  assert(!sr.ResolveObject(1, "tlsvar", "", 2).ok());
  // weak vs strong: strong definition wins deterministically
  assert(sr.Define(SymbolEntry{{2, "dup", SymbolType::Func, ""}, 2, 0x3000, 8,
                               SymbolBinding::Weak, true})
             .ok());
  assert(sr.Define(SymbolEntry{{2, "dup", SymbolType::Func, ""}, 3, 0x4000, 8,
                               SymbolBinding::Global, true})
             .ok());
  assert(sr.ResolveFunction(2, "dup", "", 9).ok().value.value == 0x4000);

  // 5. Relocations: relative applied, missing dep deferred, stub policy.
  RelocationEngine re;
  uint64_t out = 0;
  Relocation rel{0x5000, RelocType::Relative, 0, 0x10};
  assert(re.ApplyOne(rel, std::nullopt, true, 0x4000, false, out) ==
         RelocResult::Applied);
  assert(out == 0x4010);
  Relocation js{0x5008, RelocType::JumpSlot, 3, 0};
  assert(re.ApplyOne(js, std::nullopt, false, 0x4000, false, out) ==
         RelocResult::Deferred);
  Thunk th{};
  assert(re.ApplyOne(js, std::nullopt, true, 0x4000, true, out, &th) ==
         RelocResult::Stubbed);
  assert(th.is_stub);
  uint64_t v = 0;
  assert(re.ApplyOne(js, 0x7777, true, 0x4000, false, v) ==
         RelocResult::Applied);
  assert(v == 0x7777);
  Relocation tlsr{0x5010, RelocType::TpOff64, 5, 0};
  assert(re.ApplyOne(tlsr, std::nullopt, true, 0, false, out) ==
         RelocResult::Deferred);  // never silent int
  Relocation badt{0x0, RelocType::R64, 1, 0};
  assert(re.ApplyOne(badt, 0x1, true, 0, false, out) == RelocResult::Invalid);

  // 6. TLS: per-thread isolation, OOB rejected.
  TlsManager tm;
  assert(tm.RegisterTemplate(TlsTemplate{1, 64, 16, std::vector<uint8_t>(16, 7)})
             .ok());
  assert(tm.AllocThread(100, GuestTls{}).ok());
  assert(tm.AllocThread(101, GuestTls{}).ok());
  auto t1 = tm.ModuleAddress(100, 1, 0);
  auto t2 = tm.ModuleAddress(101, 1, 0);
  assert(t1.ok() && t2.ok() && t1.value != t2.value);
  assert(!tm.ModuleAddress(100, 1, 9999).ok());
  assert(tm.FsBaseForThread(100).ok());

  // 7. ABI: decode regs, variadic guard, guest-pointer validation, TLS op.
  AbiAdapter abi;
  GuestCallFrame fr;
  fr.gpr = {1, 2, 3, 0, 0, 0};
  Signature sig{{ArgClass::Gpr, ArgClass::Gpr, ArgClass::Gpr}, ArgClass::Gpr,
                false};
  assert(abi.Decode(fr, sig).ok());
  Signature big{{ArgClass::Gpr, ArgClass::Gpr, ArgClass::Gpr, ArgClass::Gpr,
                 ArgClass::Gpr, ArgClass::Gpr, ArgClass::Gpr},
                ArgClass::Gpr, false};
  assert(!abi.Decode(fr, big).ok());
  GuestMemory mem;
  assert(mem.Map(0x10000, 0x4000, PageState::ReadWrite).ok());
  assert(abi.ValidateGuestPointer(mem, 0x10000, 8, Access::Read).ok());
  assert(!abi.ValidateGuestPointer(mem, 0x90000, 8, Access::Read).ok());
  assert(abi.TlsOpGet(tm, TlsOp::GuestTcbGet, 100, 1, 0).ok());
  assert(abi.TlsOpGet(tm, TlsOp::GuestTlsGet, 100, 1, 0).ok());

  // 8. FaultBoundary: exception -> InternalInvariant + record.
  FaultManager fm;
  GuestExecutionContext ctx{100, 1, 0x1234, {}};
  auto r = FaultBoundary::Invoke<int>(ctx, fm, FaultKind::CpuPageFault,
                                     []() -> Result<int> {
                                       throw std::runtime_error("boom");
                                       return Result<int>::Ok(0);
                                     });
  assert(!r.ok() && r.error == RuntimeError::InternalInvariant);
  assert(fm.TotalFaults() == 1);

  std::puts("test_linker OK");
  return 0;
}
