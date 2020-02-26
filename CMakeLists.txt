cmake_minimum_required(VERSION 3.13)

# Put binaries at the root of the build folder
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif(NOT CMAKE_BUILD_TYPE)

# Export compile commands for the lsp
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# =============================================================
# Setup C++

# Setup the vcpkg toolchain to get packages
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
    CACHE STRING "")
endif()

# Use c++ 17 flags and fix system includes with clang-cl.exe
IF (WIN32)
  # Use clang-cl.exe MSVC-like flags
  add_compile_options(--driver-mode=cl)
  add_compile_options(/std:c++17)
  set(CMAKE_INCLUDE_SYSTEM_FLAG_CXX "-imsvc ")
ELSE()
  add_compile_options(-std=c++17)
ENDIF()

# Treat warnings as error
add_compile_options(-fcolor-diagnostics -fansi-escape-codes -Wall -Wextra -Wno-c++98-compat -Wno-exit-time-destructors -Wno-global-constructors)

# Debug specific config
IF(CMAKE_BUILD_TYPE MATCHES Debug)
  # static analysis tools
  # find_program(CMAKE_CXX_CPPCHECK NAMES cppcheck)
  # if (CMAKE_CXX_CPPCHECK)
  #   list(
  #     APPEND CMAKE_CXX_CPPCHECK
  #     "--std=c++17"
  #     "--enable=all"
  #     )
  # endif()

  # Too many warnings for now...
  set(CMAKE_CXX_CLANG_TIDY
    clang-tidy;
    -header-filter=.;
    -checks=modernize-*,-llvm-header-guard,-modernize-use-trailing-return-type,-readability-convert-member-functions-to-static;
    )
    #-warnings-as-errors=*;)


# Release specific config
ELSEIF(CMAKE_BUILD_TYPE MATCHES Release)

ENDIF()


# =============================================================
# MAIN TARGET
project(test-vulkan LANGUAGES CXX)

# List the source files
set(TEST_VULKAN_FILES
  src/main.cpp
  src/window.cpp
  src/app.cpp
  src/tools.cpp
  src/renderer/vlk_context.cpp
  src/renderer/renderer.cpp
  src/renderer/hl_api.cpp
  src/renderer/hl_api_resources.cpp
  src/renderer/hl_api_draw.cpp
  )

# And add the main executable
add_executable(test-vulkan ${TEST_VULKAN_FILES})

# --------------
# Setup libraries
add_definitions(
  -DNOMINMAX #ty windows....

  -DVULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1

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
  )

target_include_directories(test-vulkan
  SYSTEM PUBLIC third_party/
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
   "shaders/*.geom"
   )

 foreach(GLSL ${GLSL_SOURCE_FILES})
   get_filename_component(FILE_NAME ${GLSL} NAME)
   set(SPIRV "${PROJECT_BINARY_DIR}/shaders/${FILE_NAME}.spv")
   add_custom_command(
     OUTPUT ${SPIRV}
     COMMAND glslc ${GLSL} -o ${SPIRV}
     DEPENDS ${GLSL})
   list(APPEND SPIRV_BINARY_FILES ${SPIRV})
 endforeach(GLSL)

 add_custom_target(
   shaders
   DEPENDS ${SPIRV_BINARY_FILES}
   )

 add_dependencies(test-vulkan shaders)
# =============================================================
