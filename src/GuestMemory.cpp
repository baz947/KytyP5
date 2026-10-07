#include "kyty/GuestMemory.h"
#include <cstring>

namespace kyty {

GuestMemory::GuestMemory() = default;

Result<void> GuestMemory::CheckRange(GuestAddress addr, size_t size) const {
  if (size == 0)
    return Result<void>::Fail(RuntimeError::InvalidArgument, "size==0");
  if (addr >= kAddrSpaceSize || size >= kAddrSpaceSize ||
      addr + size > kAddrSpaceSize || addr + size < addr)
    return Result<void>::Fail(RuntimeError::InvalidArgument,
                              "address out of range");
  return Result<void>::Ok();
}

Result<void> GuestMemory::Map(GuestAddress addr, size_t size,
                              PageState state) {
  if (auto c = CheckRange(addr, size); !c.ok()) return c;
  if (addr % kPageSize != 0 || size % kPageSize != 0)
    return Result<void>::Fail(RuntimeError::InvalidArgument,
                              "Map must be page-aligned");
  if (state == PageState::Unmapped)
    return Result<void>::Fail(RuntimeError::InvalidArgument,
                              "Map with Unmapped state");

  // Lazily create backing store covering [addr, addr+size).
  if (backing_.empty()) {
    base_ = addr;
    backed_size_ = size;
    backing_.resize(size, 0);
  } else {
    GuestAddress end = addr + size, cur_end = base_ + backed_size_;
    if (addr < base_ || end > cur_end) {
      // Grow to union (simple model; real impl uses host mappings).
      GuestAddress nb = addr < base_ ? addr : base_;
      GuestAddress ne = end > cur_end ? end : cur_end;
      std::vector<uint8_t> nbuf(size_t(ne - nb), 0);
      // copy old
      std::memcpy(nbuf.data() + (base_ - nb), backing_.data(),
                  backing_.size());
      backing_.swap(nbuf);
      base_ = nb;
      backed_size_ = size_t(ne - nb);
    }
  }

  uint8_t perms = 0;
  switch (state) {
    case PageState::ReadOnly:
      perms = uint8_t(Access::Read);
      break;
    case PageState::ReadWrite:
      perms = uint8_t(Access::Read) | uint8_t(Access::Write);
      break;
    case PageState::Executable:
      perms = uint8_t(Access::Read) | uint8_t(Access::Execute);
      break;
    case PageState::GpuVisible:
      perms = uint8_t(Access::Read) | uint8_t(Access::Write);
      break;
    case PageState::Committed:
      perms = uint8_t(Access::Read) | uint8_t(Access::Write);
      break;
    default:
      perms = uint8_t(Access::Read) | uint8_t(Access::Write);
      break;
  }
  for (uint64_t p = PageBase(addr); p < addr + size; p += kPageSize) {
    pages_[p] = PageInfo{state, perms, ++mapping_version_};
  }
  ++mapping_version_;
  return Result<void>::Ok();
}

Result<void> GuestMemory::Unmap(GuestAddress addr, size_t size) {
  if (auto c = CheckRange(addr, size); !c.ok()) return c;
  if (addr % kPageSize != 0 || size % kPageSize != 0)
    return Result<void>::Fail(RuntimeError::InvalidArgument,
                              "Unmap must be page-aligned");
  for (uint64_t p = PageBase(addr); p < addr + size; p += kPageSize) {
    pages_.erase(p);
  }
  ++mapping_version_;
  return Result<void>::Ok();
}

Result<void> GuestMemory::Protect(GuestAddress addr, size_t size,
                                  uint8_t perms) {
  if (auto c = CheckRange(addr, size); !c.ok()) return c;
  for (uint64_t p = PageBase(addr); p < addr + size; p += kPageSize) {
    auto it = pages_.find(p);
    if (it == pages_.end())
      return Result<void>::Fail(RuntimeError::InvalidGuestMemory,
                                "Protect on unmapped page");
    it->second.perms = perms;
    it->second.version = ++mapping_version_;
  }
  return Result<void>::Ok();
}

Result<void> GuestMemory::Synchronize(GuestAddress addr, size_t size) {
  if (auto c = CheckRange(addr, size); !c.ok()) return c;
  // Bump GPU version so caches keyed on (addr,range,version) invalidate.
  ++gpu_version_;
  return Result<void>::Ok();
}

Result<MemoryTranslation> GuestMemory::Translate(GuestAddress addr, size_t size,
                                                 Access access) const {
  if (auto c = CheckRange(addr, size); !c.ok())
    return Result<MemoryTranslation>::Fail(c.error, c.detail);
  // Page-aware: every page in range must be present with right perms.
  for (uint64_t p = PageBase(addr); p < addr + size; p += kPageSize) {
    auto it = pages_.find(p);
    if (it == pages_.end())
      return Result<MemoryTranslation>::Fail(
          RuntimeError::InvalidGuestMemory, "unmapped page");
    if ((it->second.perms & uint8_t(access)) == 0)
      return Result<MemoryTranslation>::Fail(
          RuntimeError::InvalidGuestMemory, "permission denied");
    if (it->second.state == PageState::Evicted ||
        it->second.state == PageState::Faulting)
      return Result<MemoryTranslation>::Fail(
          RuntimeError::InvalidGuestMemory, "page not resident");
  }
  if (backing_.empty() || addr < base_ || addr + size > base_ + backed_size_)
    return Result<MemoryTranslation>::Fail(
        RuntimeError::HostMemoryFailure, "no host mapping");
  MemoryTranslation t;
  t.ok = true;
  t.host_offset = size_t(addr - base_);
  t.version = mapping_version_;
  return Result<MemoryTranslation>::Ok(t);
}

Result<void> GuestMemory::Read(GuestAddress addr, void* out,
                               size_t size) const {
  if (!out)
    return Result<void>::Fail(RuntimeError::InvalidArgument, "null out");
  auto t = Translate(addr, size, Access::Read);
  if (!t.ok()) return Result<void>::Fail(t.error, t.detail);
  std::memcpy(out, backing_.data() + t.value.host_offset, size);
  return Result<void>::Ok();
}

Result<void> GuestMemory::Write(GuestAddress addr, const void* in,
                                size_t size) {
  if (!in)
    return Result<void>::Fail(RuntimeError::InvalidArgument, "null in");
  auto t = Translate(addr, size, Access::Write);
  if (!t.ok()) return Result<void>::Fail(t.error, t.detail);
  std::memcpy(backing_.data() + t.value.host_offset, in, size);
  ++cpu_version_;
  return Result<void>::Ok();
}

PageInfo GuestMemory::Query(GuestAddress addr) const {
  auto it = pages_.find(PageBase(addr));
  if (it == pages_.end()) return PageInfo{};
  return it->second;
}

}  // namespace kyty
