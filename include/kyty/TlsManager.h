#pragma once
// TLS manager — README §9 (P0). Guest TLS is separate from Host TLS.
// fs: rewriting lives behind AbiAdapter, not scattered in the loader.
#include <cstdint>
#include <unordered_map>
#include <vector>
#include "kyty/Compat.h"
#include "kyty/GuestModule.h"

namespace kyty::linker {

struct TlsTemplate {
  ModuleId owner = 0;
  size_t size = 0;
  size_t align = 16;
  std::vector<uint8_t> image;  // initial content (filesz bytes)
};

struct GuestTls {
  uint64_t tcb = 0;           // guest address of TCB
  uint64_t errno_addr = 0;    // guest address of per-thread errno
  uint64_t locale_addr = 0;
  uint64_t runtime_state = 0;
};

class TlsManager {
 public:
  Result<void> RegisterTemplate(TlsTemplate t);
  // Allocates a thread-local block; returns guest base address (simulated
  // guest VA namespace owned by this manager starting at kTlsBase).
  Result<uint64_t> AllocThread(uint64_t thread_id, GuestTls info);
  Result<uint64_t> ModuleAddress(uint64_t thread_id, ModuleId mod,
                                 uint64_t offset) const;
  Result<GuestTls> Info(uint64_t thread_id) const;
  // Host-visible: the value AbiAdapter programs into guest fs-base.
  Result<uint64_t> FsBaseForThread(uint64_t thread_id) const;
  void Clear();

 private:
  static constexpr uint64_t kTlsBase = uint64_t{1} << 40;
  uint64_t next_offset_ = 0;
  std::unordered_map<ModuleId, TlsTemplate> templates_;
  struct ThreadBlock {
    GuestTls info;
    uint64_t base = 0;
    // per-module offset within block
    std::unordered_map<ModuleId, uint64_t> mod_off;
  };
  std::unordered_map<uint64_t, ThreadBlock> threads_;
  static size_t AlignUp(size_t v, size_t a) {
    return (v + a - 1) & ~(a - 1);
  }
};

}  // namespace kyty::linker
