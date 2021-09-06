cmake_minimum_required(VERSION 3.19)

set(CMAKE_CXX_STANDARD 17)

project(test-vulkan LANGUAGES C CXX)

find_package(Vulkan REQUIRED)
find_package(Threads)

# Global C++ flags
add_compile_options(
  # colored output when using ninja generator
  $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-fcolor-diagnostics>
  $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-fansi-escape-codes>


  # Disable warnings for external headers
  $<$<CXX_COMPILER_ID:MSVC>:/external:anglebrackets>
  $<$<CXX_COMPILER_ID:MSVC>:/external:W0>
  $<$<CXX_COMPILER_ID:MSVC>:/arch:AVX2>
  )

# C++ flags for the project's code
set(APP_CXX_FLAGS
  # Warning level
  $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-Wall>
  $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-Wextra>
  $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-Wpedantic>

  $<$<CXX_COMPILER_ID:MSVC>:/W4>
  $<$<CXX_COMPILER_ID:MSVC>:/permissive->

  # 0-sized array
  $<$<CXX_COMPILER_ID:MSVC>:/wd4200>
  $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-Wno-zero-length-array>

  # nameless union, struct
  $<$<CXX_COMPILER_ID:MSVC>:/wd4201>
  $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-Wno-nested-anon-types>
  $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-Wno-gnu-anonymous-struct>

  # build time profiling
  # $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-ftime-trace>
  $<$<CXX_COMPILER_ID:MSVC>:/MP>
  )

# C++ flags for third party libraries
set(THIRD_PARTY_CXX_FLAGS
  # Disable warnings for third-party libs
  $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-Wno-everything>
  $<$<CXX_COMPILER_ID:MSVC>:/W0>
  $<$<CXX_COMPILER_ID:MSVC>:/MP>
  )

add_subdirectory(third_party)
add_subdirectory(exo)
add_subdirectory(cross)
add_subdirectory(engine)
