cmake_minimum_required(VERSION 3.17)
include(FetchContent)

# Setup the vcpkg toolchain to get packages
if(DEFINED ENV{VCPKG_ROOT})
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}) # Put binaries at the root of the build folder
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

cmake_policy(SET CMP0091 NEW) # makes MSVC_RUNTIME_LIBRARY variable work
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>") # Specify MSVC runtime to link libraries correctly

project(test-vulkan LANGUAGES CXX)

add_definitions(
  #ty windows....
  -DNOMINMAX
  -DVC_EXTRALEAN
  -DWIN32_LEAN_AND_MEAN
  -DUNICODE
  -DVK_USE_PLATFORM_WIN32_KHR

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
find_package(nlohmann_json CONFIG REQUIRED)
find_package(unofficial-vulkan-memory-allocator CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)

# Main executable
add_executable(test-vulkan
  src/main.cpp
  src/window_win32.cpp
  src/camera.cpp
  src/app.cpp
  src/tools.cpp
  src/timer.cpp
  src/gltf.cpp
  src/file_watcher.cpp
  src/renderer/vlk_context.cpp
  src/renderer/renderer.cpp
  src/renderer/render_graph.cpp
  src/renderer/hl_api.cpp
  src/renderer/hl_api_resources.cpp
  src/renderer/hl_api_draw.cpp
  )

target_compile_options( test-vulkan PUBLIC
  -fcolor-diagnostics -fansi-escape-codes # colored output when using ninja
  -Werror -Wall -Wextra -Wno-c++98-compat -Wno-exit-time-destructors -Wno-global-constructors # warnings
  )

target_link_libraries(test-vulkan
  PRIVATE
  Vulkan::Vulkan
  glm
  nlohmann_json
  nlohmann_json::nlohmann_json
  unofficial::vulkan-memory-allocator::vulkan-memory-allocator
  )

if (WIN32)
  target_link_libraries(test-vulkan
    PRIVATE
    Imm32 # Win32 IME for emoji picker or other IME input
    )
elseif(UNIX)
  target_link_libraries(test-vulkan
    PRIVATE
    Threads::Threads
    Fontconfig::Fontconfig
    ${X11_LIBRARIES}
    ${X11_Xi_LIB}
    tbb
    )
endif()

target_include_directories(test-vulkan
  PUBLIC include/
  SYSTEM PUBLIC third_party/
  )

# SHADERS

file(GLOB_RECURSE GLSL_SOURCE_FILES
  "shaders/*.frag"
  "shaders/*.vert"
  "shaders/*.geom"
  "shaders/*.comp"
  )

foreach(GLSL ${GLSL_SOURCE_FILES})
  get_filename_component(FILE_NAME ${GLSL} NAME)
  set(SPIRV "${PROJECT_BINARY_DIR}/shaders/${FILE_NAME}.spv")
  add_custom_command(
    OUTPUT ${SPIRV}
    COMMAND glslc --target-env=vulkan1.2 -std=460 ${GLSL} -o ${SPIRV} -I "../shaders/include" # relative to build directory
    DEPENDS ${GLSL})
  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(
  shaders
  DEPENDS ${SPIRV_BINARY_FILES}
  )

add_dependencies(test-vulkan shaders)

# ImGUI

add_library(imgui
  third_party/imgui/imgui.cpp
  third_party/imgui/imgui_demo.cpp
  third_party/imgui/imgui_draw.cpp
  third_party/imgui/imgui_widgets.cpp
  )

target_link_libraries(test-vulkan
  PRIVATE
  imgui
  )
