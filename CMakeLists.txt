cmake_minimum_required(VERSION 3.17)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}) # Put binaries at the root of the build folder
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_COMPILER "clang")

project(test-vulkan LANGUAGES CXX)

# set(CMAKE_CXX_CLANG_TIDY clang-tidy)

add_definitions(
  #ty windows....
  -DNOMINMAX
  -DVC_EXTRALEAN
  -DWIN32_LEAN_AND_MEAN
  -DUNICODE

  #imgui
  -DIMGUI_USER_CONFIG="imgui_config.h"

  # VMA debug
  -DVMA_DEBUG_INITIALIZE_ALLOCATIONS=1
  -DVMA_DEBUG_MARGIN=16
  -DVMA_DEBUG_DETECT_CORRUPTION=1
  )

add_compile_options(
  -fcolor-diagnostics -fansi-escape-codes # colored output when using ninja
  )

#
# Main executable
#

add_executable(test-vulkan
  src/main.cpp
  src/base/vectors.cpp
  src/platform/window_win32.cpp
  src/camera.cpp
  src/ui.cpp
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

target_compile_options(test-vulkan
  PUBLIC
  -Werror -Wall -Wextra# warnings
)

target_include_directories(test-vulkan PUBLIC include/)
target_include_directories(test-vulkan SYSTEM PUBLIC third_party/)

# System libraries
if (WIN32)
  target_link_libraries(test-vulkan
    PRIVATE
    Imm32 # Win32 IME for emoji picker or other IME input
    )
elseif (UNIX)
  target_link_libraries(test-vulkan
    PRIVATE
    Threads::Threads
    Fontconfig::Fontconfig
    ${X11_LIBRARIES}
    ${X11_Xi_LIB}
    tbb
    )
endif()


# C++ Libraires
find_package(Vulkan REQUIRED)

target_link_libraries(test-vulkan PRIVATE Vulkan::Vulkan)

#
# SHADERS
#

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

#
# ImGUI
#

add_library(imgui
  third_party/imgui/imgui.cpp
  third_party/imgui/imgui_demo.cpp
  third_party/imgui/imgui_draw.cpp
  third_party/imgui/imgui_widgets.cpp
  )

target_include_directories(imgui
  PUBLIC include/
  )

target_link_libraries(test-vulkan
  PRIVATE
  imgui
  )

#
# simdjson
#
add_library(simdjson third_party/simdjson/simdjson.cpp)
target_link_libraries(test-vulkan PRIVATE simdjson)
