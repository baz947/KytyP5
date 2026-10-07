#pragma once
// Global Compatibility Contract — README §1
// Never use `return 0` as universal unsupported-function policy.
// Every API returns Result<T> with explicit RuntimeError.
#include <cstdint>
#include <string>
#include <variant>

namespace kyty {

enum class CompatStatus : uint8_t {
  Exact = 0,
  Compatible,
  Emulated,
  Partial,
  SafeStub,
  Unsupported,
  WrongABI,
  Broken,
};

enum class GpuCapability : uint8_t {
  Native = 0,
  Lowered,
  Emulated,
  Fallback,
  Unavailable,
};

enum class RuntimeError : uint32_t {
  None = 0,
  InvalidArgument,
  InvalidGuestMemory,
  MissingDependency,
  UnresolvedImport,
  WrongABI,
  UnsupportedFeature,
  InvalidResource,
  ShaderFailure,
  HostMemoryFailure,
  HostGpuFailure,
  Timeout,
  Deadlock,
  InternalInvariant,
};

inline const char* ToString(RuntimeError e) {
  switch (e) {
    case RuntimeError::None: return "None";
    case RuntimeError::InvalidArgument: return "InvalidArgument";
    case RuntimeError::InvalidGuestMemory: return "InvalidGuestMemory";
    case RuntimeError::MissingDependency: return "MissingDependency";
    case RuntimeError::UnresolvedImport: return "UnresolvedImport";
    case RuntimeError::WrongABI: return "WrongABI";
    case RuntimeError::UnsupportedFeature: return "UnsupportedFeature";
    case RuntimeError::InvalidResource: return "InvalidResource";
    case RuntimeError::ShaderFailure: return "ShaderFailure";
    case RuntimeError::HostMemoryFailure: return "HostMemoryFailure";
    case RuntimeError::HostGpuFailure: return "HostGpuFailure";
    case RuntimeError::Timeout: return "Timeout";
    case RuntimeError::Deadlock: return "Deadlock";
    case RuntimeError::InternalInvariant: return "InternalInvariant";
    default: return "Unknown";
  }
}

template <typename T>
struct Result {
  RuntimeError error = RuntimeError::None;
  T value{};
  std::string detail;

  static Result Ok(T v) { return {RuntimeError::None, std::move(v), {}}; }
  static Result Fail(RuntimeError e, std::string d = {}) {
    return {e, T{}, std::move(d)};
  }
  [[nodiscard]] bool ok() const { return error == RuntimeError::None; }
};

template <>
struct Result<void> {
  RuntimeError error = RuntimeError::None;
  std::string detail;
  static Result Ok() { return {RuntimeError::None, {}}; }
  static Result Fail(RuntimeError e, std::string d = {}) {
    return {e, std::move(d)};
  }
  [[nodiscard]] bool ok() const { return error == RuntimeError::None; }
};

}  // namespace kyty
