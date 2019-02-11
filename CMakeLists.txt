cmake_minimum_required(VERSION 3.13)
project(test-vulkan LANGUAGES CXX)

if(WIN32)
  add_compile_options(/std:c++17)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})


find_package(Vulkan REQUIRED)

# Don't build useless stuff for glfw
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(lib/glfw-3.2.1)

set(glm_DIR lib/glm/build)
find_package(glm REQUIRED)

set(SOURCE
  src/vk_mem_alloc.cpp
  src/main.cpp
)

add_executable(test-vulkan ${SOURCE})

target_link_libraries(test-vulkan
  Vulkan::Vulkan
  glfw
  glm
  )

target_include_directories(test-vulkan
  PUBLIC lib/glfw-3.2.1/include
  PUBLIC lib/VulkanMemoryAllocator/include
  )
