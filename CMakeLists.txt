cmake_minimum_required(VERSION 3.17)

cmake_policy(SET CMP0091 NEW) # makes MSVC_RUNTIME_LIBRARY variable work

# Setup the vcpkg toolchain to get packages
if(DEFINED ENV{VCPKG_ROOT})
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()


set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}) # Put binaries at the root of the build folder
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(test-vulkan LANGUAGES CXX)

add_compile_options(
  -fcolor-diagnostics -fansi-escape-codes # colored output when using ninja
  -Wall -Wextra -Wno-c++98-compat -Wno-exit-time-destructors -Wno-global-constructors # warnings
  )

# Enable ASAN
if(0 AND (NOT WIN32 OR CMAKE_BUILD_TYPE MATCHES Release))
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
  set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} -fsanitize=address")
endif ()

add_definitions(
  -DNOMINMAX #ty windows....
  -DVULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
  -DGLFW_INCLUDE_VULKAN
  -DGLM_FORCE_RADIANS
  -DGLM_FORCE_DEPTH_ZERO_TO_ONE
  -DVMA_DEBUG_INITIALIZE_ALLOCATIONS=1
  -DVMA_DEBUG_MARGIN=16
  -DVMA_DEBUG_DETECT_CORRUPTION=1

  -DENABLE_IMGUI
  )

# set(CMAKE_CXX_CLANG_TIDY clang-tidy)

# C++ Libraires, need to be installed with vcpkg
find_package(Vulkan REQUIRED)
find_package(glm REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(unofficial-vulkan-memory-allocator CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)

# Main executable
add_executable(test-vulkan
  src/main.cpp
  src/window.cpp
  src/camera.cpp
  src/app.cpp
  src/tools.cpp
  src/timer.cpp
  src/gltf.cpp
  src/file_watcher.cpp
  src/renderer/vlk_context.cpp
  src/renderer/renderer.cpp
  src/renderer/hl_api.cpp
  src/renderer/hl_api_resources.cpp
  src/renderer/hl_api_draw.cpp
  )

target_link_libraries(test-vulkan
  PRIVATE
  Vulkan::Vulkan
  glfw
  glm
  nlohmann_json
  nlohmann_json::nlohmann_json
  unofficial::vulkan-memory-allocator::vulkan-memory-allocator
  imgui::imgui
  )

target_include_directories(test-vulkan
  PUBLIC include/
  SYSTEM PUBLIC third_party/
  )

# SHADERS
file(GLOB_RECURSE GLSL_SOURCE_FILES
  "shaders/*.frag"
  "shaders/*.vert"
  "shaders/*.geom"
  )

foreach(GLSL ${GLSL_SOURCE_FILES})
  get_filename_component(FILE_NAME ${GLSL} NAME)
  set(SPIRV "${PROJECT_BINARY_DIR}/shaders/${FILE_NAME}.spv")
  add_custom_command(
    OUTPUT ${SPIRV}
    COMMAND glslc ${GLSL} -o ${SPIRV} -I "../shaders/include" # relative to build directory
    DEPENDS ${GLSL})
  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(
  shaders
  DEPENDS ${SPIRV_BINARY_FILES}
  )

add_dependencies(test-vulkan shaders)

# GTest executable
# add_executable(tests
#   tests/arena.cpp
#   )
#
# target_link_libraries(tests
#   GTest::gtest
#   GTest::gtest_main
#   )
#
# target_include_directories(tests
#   PUBLIC include/
#   SYSTEM PUBLIC third_party/
#   )
#
# enable_testing()
# include(GoogleTest)
# gtest_discover_tests(tests)
