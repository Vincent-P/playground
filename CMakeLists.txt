cmake_minimum_required(VERSION 3.19)

set(CMAKE_CXX_STANDARD 17)

project(test-vulkan LANGUAGES CXX)

find_package(Vulkan REQUIRED)
find_package(Threads)

set(APP_CXX_FLAGS
  $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:-Wall>
  $<$<CXX_COMPILER_ID:MSVC>:/W4>
  $<$<CXX_COMPILER_ID:MSVC>:/wd4200> # 0-sized array
  $<$<CXX_COMPILER_ID:MSVC>:/wd4201> # nameless union
  $<$<CXX_COMPILER_ID:MSVC>:/permissive->
  )

set(THIRD_PARTY_CXX_FLAGS
  $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:-Wno-everything>
  $<$<CXX_COMPILER_ID:MSVC>:/W0>
  )

add_subdirectory(third_party)
add_subdirectory(engine)
