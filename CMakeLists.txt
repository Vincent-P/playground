cmake_minimum_required(VERSION 3.13)

# Setup the vcpkg toolchain
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
    CACHE STRING "")
endif()

# Export compile commands for the lsp
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# Put binaries at the root of the build folder
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})


# Setup C++
# Uncomment to enabled cppcheck, it is disabled because it checks vulkan.hpp, glfw and unwanted libraries
# set(CMAKE_CXX_CPPCHECK "cppcheck")


IF (WIN32)
    add_compile_options(/std:c++17)
ELSE()
    add_compile_options(-std=c++17)
ENDIF()

add_compile_options(-fcolor-diagnostics -Werror -Wall -Wextra -Wno-c++98-compat -Wno-exit-time-destructors -Wno-global-constructors)

# =============================================================
# MAIN TARGET
project(test-vulkan LANGUAGES CXX)

# List the source files
set(TEST_VULKAN_FILES
  src/buffer.cpp
  src/gui.cpp
  src/image.cpp
  src/main.cpp
  src/model.cpp
  src/renderer.cpp
  src/timer.cpp
  src/tiny_gltf.cpp
  src/tools.cpp
  src/vk_mem_alloc.cpp
  src/vulkan_context.cpp
  )

# And add the main executable
add_executable(test-vulkan ${TEST_VULKAN_FILES})

# --------------
# Setup libraries
add_definitions(
#  -DDEBUG_MEMORY_LEAKS
  -DGLFW_INCLUDE_VULKAN
  -DGLM_FORCE_RADIANS
  -DGLM_FORCE_DEPTH_ZERO_TO_ONE
  -DVMA_DEBUG_INITIALIZE_ALLOCATIONS=1
  -DVMA_DEBUG_MARGIN=16
  -DVMA_DEBUG_DETECT_CORRUPTION=1
  )

find_package(Vulkan REQUIRED)
find_package(glm REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(unofficial-vulkan-memory-allocator CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)

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
# --------------

# Setup the include directories
target_include_directories(test-vulkan
  PUBLIC include/
  SYSTEM third_party/tinygltf
  )

# Add a run target to launch the executable
add_custom_target(run
  COMMAND test-vulkan
  DEPENDS test-vulkan
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  )

# =============================================================

# =============================================================
# SHADERS
file(GLOB_RECURSE GLSL_SOURCE_FILES
  "shaders/*.frag"
  "shaders/*.vert"
  )

foreach(GLSL ${GLSL_SOURCE_FILES})
  get_filename_component(FILE_NAME ${GLSL} NAME)
  set(SPIRV "${PROJECT_BINARY_DIR}/shaders/${FILE_NAME}.spv")
  add_custom_command(
    OUTPUT ${SPIRV}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/shaders/"
    COMMAND glslc ${GLSL} -o ${SPIRV}
    DEPENDS ${GLSL})
  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(
  shaders
  DEPENDS ${SPIRV_BINARY_FILES}
  )

add_dependencies(test-vulkan shaders)

add_custom_command(TARGET test-vulkan
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:test-vulkan>/shaders/"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
  "${PROJECT_BINARY_DIR}/shaders"
  "$<TARGET_FILE_DIR:test-vulkan>/shaders"
  )
# =============================================================
