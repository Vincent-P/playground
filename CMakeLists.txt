cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 17)

project(test-vulkan LANGUAGES C CXX)

find_package(Vulkan REQUIRED)
find_package(Threads)

# add_compile_definitions(ENABLE_DOCTEST)

# Global C++ flags
add_compile_options(
  # colored output when using ninja generator
  $<$<CXX_COMPILER_ID:Clang,AppleClang>:-fcolor-diagnostics>
  $<$<CXX_COMPILER_ID:Clang,AppleClang>:-fansi-escape-codes>


  # Disable warnings for external headers
  $<$<CXX_COMPILER_ID:MSVC>:/external:anglebrackets>
  $<$<CXX_COMPILER_ID:MSVC>:/external:W0>

  # Optimize for AVX2
  $<$<CXX_COMPILER_ID:MSVC>:/arch:AVX2>
  )

# C++ flags for the project's code
# https://github.com/cpp-best-practices/cppbestpractices/blob/master/02-Use_the_Tools_Available.md#compilers
set(APP_CXX_FLAGS
  # Warning level
  $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-Wall>
  $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-Wextra>
  $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-Wpedantic>
  $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-Wshadow>
  $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-Wconversion>
  $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-Wnon-virtual-dtor>


  $<$<CXX_COMPILER_ID:MSVC>:/W4>
  $<$<CXX_COMPILER_ID:MSVC>:/permissive->
  $<$<CXX_COMPILER_ID:MSVC>:/w14640> # Enable warning on thread un-safe static member initialization

  # 0-sized array
  $<$<CXX_COMPILER_ID:MSVC>:/wd4200>
  $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-Wno-zero-length-array>

  # nameless union, struct
  $<$<CXX_COMPILER_ID:MSVC>:/wd4201>
  $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-Wno-nested-anon-types>
  $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-Wno-gnu-anonymous-struct>

  $<$<CXX_COMPILER_ID:GNU>:-Wno-missing-field-initializers>


  # $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-fsanitize=address>
  # $<$<CXX_COMPILER_ID:MSVC>:/fsanitize=address>

  )

# C++ flags for third party libraries
set(THIRD_PARTY_CXX_FLAGS
  # Disable warnings for third-party libs
  $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-Wno-everything>
  $<$<CXX_COMPILER_ID:MSVC>:/W0>
  $<$<CXX_COMPILER_ID:MSVC>:/MP>
  )

function(SETUP_APP_TARGET TARGET)
  message(STATUS "Setting up target ${TARGET}")
  set_target_properties(${TARGET} PROPERTIES CXX_STANDARD 20)
  set_target_properties(${TARGET} PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
endfunction()

add_subdirectory(third_party)
add_subdirectory(exo)
add_subdirectory(cross)
add_subdirectory(engine)
