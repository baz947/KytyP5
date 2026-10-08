cmake_minimum_required(VERSION 3.20)
project(KytyPS5-SemanticRuntime VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- Cross-platform defaults ----
# Windows: UTF-8 sources, correct __cplusplus, no legacy warnings.
# Linux/macOS: strict warnings; macOS deployment target left to CI env.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Library: semantic core (P0 foundation + services + GPU + shader + infra)
add_library(kyty_semantic
  src/GuestMemory.cpp
  src/FaultManager.cpp
  src/GuestClock.cpp
  src/HandleManager.cpp
  src/WaitManager.cpp
  src/ElfLoader.cpp
  src/GuestModule.cpp
  src/ModuleManager.cpp
  src/SymbolResolver.cpp
  src/RelocationEngine.cpp
  src/TlsManager.cpp
  src/AbiAdapter.cpp
  src/ExecutionContext.cpp
  src/ServiceRegistry.cpp
  src/VirtualFs.cpp
  src/SaveData.cpp
  src/UserService.cpp
  src/VideoOut.cpp
  src/AudioManager.cpp
  src/PadManager.cpp
  src/NetworkNpTrophy.cpp
  src/GpuFormat.cpp
  src/CanonicalResource.cpp
  src/DescriptorDecoder.cpp
  src/ResourceTracking.cpp
  src/ResourceMaterialization.cpp
  src/WaveModel.cpp
  src/AtomicBarrierModel.cpp
  src/HostGpuCaps.cpp
  src/GpuTimeline.cpp
  src/ShaderHangDetector.cpp
  src/ShaderCore.cpp
  src/CompatDb.cpp
  src/TraceDiagnostics.cpp
  src/FuzzChaos.cpp
  src/IsaTable.cpp
  src/CorpusRunner.cpp
  src/BackendLowering.cpp
  src/CorpusCompatBridge.cpp
  src/RdnaIsaFull.cpp
  src/P2PerfPresent.cpp
  src/SoakRunner.cpp
  src/SpirvBackend.cpp
)
target_include_directories(kyty_semantic PUBLIC include)

if(MSVC)
  target_compile_options(kyty_semantic PRIVATE
    /W4 /permissive- /utf-8 /Zc:__cplusplus /EHsc)
else()
  target_compile_options(kyty_semantic PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
  if(APPLE)
    target_compile_options(kyty_semantic PRIVATE -Wno-deprecated-declarations)
  endif()
endif()

# Threads are required by WaitManager (portable: win32 pthreads / std).
find_package(Threads REQUIRED)
target_link_libraries(kyty_semantic PUBLIC Threads::Threads)

# Deterministic output layout on all generators (VS/Xcode/Ninja/Make).
set_target_properties(kyty_semantic PROPERTIES
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

enable_testing()

# C++ regression tests (plain assert, no external deps -> CI friendly)
foreach(t test_memory test_fault test_clock test_handle test_wait test_linker test_services test_gpu test_shader test_infra test_isa_corpus test_backend_bridge test_p2_rdna test_soak test_spirv)
  add_executable(${t} tests/cpp/${t}.cpp)
  target_link_libraries(${t} PRIVATE kyty_semantic)
  if(MSVC)
    target_compile_options(${t} PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus /EHsc)
  else()
    target_compile_options(${t} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
  set_target_properties(${t} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
  add_test(NAME ${t} COMMAND ${t})
  set_tests_properties(${t} PROPERTIES TIMEOUT 60)
endforeach()

# Python mirror (runnable without a C++ toolchain) as a CTest step.
find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
  add_test(NAME python_mirror
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/run_tests.py")
  set_tests_properties(python_mirror PROPERTIES TIMEOUT 120)
endif()

include(GNUInstallDirs)
install(TARGETS kyty_semantic
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
