#include "kyty/TlsManager.h"

namespace kyty::linker {

Result<void> TlsManager::RegisterTemplate(TlsTemplate t) {
  if (t.owner == 0)
    return Result<void>::Fail(RuntimeError::InvalidArgument, "bad owner");
  if (t.align == 0 || (t.align & (t.align - 1)) != 0)
    return Result<void>::Fail(RuntimeError::InvalidArgument,
                              "align must be pow2");
  if (t.image.size() > t.size)
    return Result<void>::Fail(RuntimeError::InvalidArgument,
                              "image bigger than memsz");
  templates_[t.owner] = std::move(t);
  return Result<void>::Ok();
}

Result<uint64_t> TlsManager::AllocThread(uint64_t thread_id, GuestTls info) {
  if (thread_id == 0)
    return Result<uint64_t>::Fail(RuntimeError::InvalidArgument,
                                   "bad thread");
  if (threads_.count(thread_id))
    return Result<uint64_t>::Fail(RuntimeError::InvalidArgument,
                                   "thread TLS exists");
  ThreadBlock blk;
  blk.info = info;
  blk.base = kTlsBase + next_offset_;
  size_t cursor = 0;
  for (auto& [mod, t] : templates_) {
    cursor = AlignUp(cursor, t.align);
    blk.mod_off[mod] = cursor;
    cursor += AlignUp(t.size, t.align);
  }
  // Reserve TCB + cursor, page-align the stride per thread.
  size_t stride = AlignUp(cursor + 64, 0x4000);
  next_offset_ += stride;
  threads_[thread_id] = std::move(blk);
  auto& b = threads_[thread_id];
  b.info.tcb = b.base;  // TCB at block base
  return Result<uint64_t>::Ok(b.base);
}

Result<uint64_t> TlsManager::ModuleAddress(uint64_t thread_id, ModuleId mod,
                                           uint64_t offset) const {
  auto it = threads_.find(thread_id);
  if (it == threads_.end())
    return Result<uint64_t>::Fail(RuntimeError::InvalidArgument,
                                   "no thread TLS");
  auto jt = it->second.mod_off.find(mod);
  if (jt == it->second.mod_off.end())
    return Result<uint64_t>::Fail(RuntimeError::MissingDependency,
                                   "no TLS template for module");
  auto kt = templates_.find(mod);
  if (kt == templates_.end())
    return Result<uint64_t>::Fail(RuntimeError::InternalInvariant,
                                   "template vanished");
  if (offset >= kt->second.size)
    return Result<uint64_t>::Fail(RuntimeError::InvalidGuestMemory,
                                   "TLS offset OOB");
  return Result<uint64_t>::Ok(it->second.base + jt->second + offset);
}

Result<GuestTls> TlsManager::Info(uint64_t thread_id) const {
  auto it = threads_.find(thread_id);
  if (it == threads_.end())
    return Result<GuestTls>::Fail(RuntimeError::InvalidArgument,
                                   "no thread TLS");
  return Result<GuestTls>::Ok(it->second.info);
}

Result<uint64_t> TlsManager::FsBaseForThread(uint64_t thread_id) const {
  auto it = threads_.find(thread_id);
  if (it == threads_.end())
    return Result<uint64_t>::Fail(RuntimeError::InvalidArgument,
                                   "no thread TLS");
  return Result<uint64_t>::Ok(it->second.info.tcb);
}

void TlsManager::Clear() {
  templates_.clear();
  threads_.clear();
  next_offset_ = 0;
}

}  // namespace kyty::linker
