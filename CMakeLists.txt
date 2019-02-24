cmake_minimum_required(VERSION 3.13)

if(WIN32)
  add_compile_options(/std:c++17)
endif()

if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
      CACHE STRING "")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

project(test-vulkan LANGUAGES CXX)

add_definitions(
        -DGLFW_INCLUDE_VULKAN
        -DGLM_FORCE_RADIANS
        -DVMA_DEBUG_INITIALIZE_ALLOCATIONS=1
        -DVMA_DEBUG_MARGIN=16
        -DVMA_DEBUG_DETECT_CORRUPTION=1
        )

find_package(Vulkan REQUIRED)
# These are the names of the package using vcpkg on Windows
find_package(glm REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(tinyGLTF CONFIG REQUIRED)
find_package(unofficial-vulkan-memory-allocator CONFIG REQUIRED)

set(SOURCE
  src/vk_mem_alloc.cpp
  src/tiny_gltf.cpp
  src/model.cpp
  src/renderer.cpp
  src/vulkan_context.cpp
  src/main.cpp
  )

add_executable(test-vulkan ${SOURCE})
target_link_libraries(test-vulkan
  PRIVATE
  Vulkan::Vulkan
  glfw
  glm
  nlohmann_json
  nlohmann_json::nlohmann_json
  unofficial::vulkan-memory-allocator::vulkan-memory-allocator
  )

target_include_directories(test-vulkan
  PUBLIC include/
  PUBLIC ${TinyGLTF_INCLUDE_DIR}
  )
