cmake_minimum_required(VERSION 3.23)
project(playground LANGUAGES C CXX)


# Enable folders in the Solution Explorer of Visual Studio
set_property(GLOBAL PROPERTY USE_FOLDERS ON)


# Fetch Catch2
Include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.0.1 # or a later release
)
FetchContent_MakeAvailable(Catch2)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
set_target_properties(Catch2 PROPERTIES FOLDER "third_party")
set_target_properties(Catch2WithMain PROPERTIES FOLDER "third_party")

enable_testing()


# Global C++ flags
add_compile_options(
  # colored output when using ninja generator
  $<$<CXX_COMPILER_ID:Clang,AppleClang>:-fcolor-diagnostics>
  $<$<CXX_COMPILER_ID:Clang,AppleClang>:-fansi-escape-codes>
  $<$<CXX_COMPILER_ID:GNU>:-fdiagnostics-color=always>

  # Enable libc++ with clang on Linux
  $<$<AND:$<CXX_COMPILER_ID:Clang>,$<PLATFORM_ID:Linux>>:-stdlib=libc++>

  # Disable warnings for external headers
  $<$<CXX_COMPILER_ID:MSVC>:/external:anglebrackets>
  $<$<CXX_COMPILER_ID:MSVC>:/external:W0>

  # Multi-processor compilation
  $<$<CXX_COMPILER_ID:MSVC>:/MP>
)


# Global C++ macros
add_compile_definitions(
  $<$<PLATFORM_ID:Windows>:_CRT_SECURE_NO_WARNINGS>
)


# Function to setup 3rd party source code
function(SETUP_THIRDPARTY_TARGET TARGET)
  message(STATUS "Setting up third party target ${TARGET}")
  set_target_properties(${TARGET} PROPERTIES CXX_STANDARD 17)
  set_target_properties(${TARGET} PROPERTIES FOLDER "third_party")
  target_compile_options(${TARGET} PRIVATE
      # Disable warnings for third-party libs
      $<$<CXX_COMPILER_ID:Clang,GNU>:-Wno-everything>
      $<$<CXX_COMPILER_ID:MSVC>:/W0>
  )
endfunction()


function(setup_app_target_options TARGET)
  target_compile_options(${TARGET} PRIVATE
      $<$<CXX_COMPILER_ID:Clang,GNU>:-march=native>

      # Warning level
      $<$<CXX_COMPILER_ID:Clang>:-Werror> # No GNU here :)
      $<$<CXX_COMPILER_ID:Clang>:-Wall>
      $<$<CXX_COMPILER_ID:Clang,GNU>:-Wextra>
      $<$<CXX_COMPILER_ID:Clang,GNU>:-Wpedantic>
      $<$<CXX_COMPILER_ID:Clang,GNU>:-Wshadow>
      $<$<CXX_COMPILER_ID:Clang,GNU>:-Wconversion>
      $<$<CXX_COMPILER_ID:Clang,GNU>:-Wnon-virtual-dtor>
      $<$<CXX_COMPILER_ID:Clang,GNU>:-Woverloaded-virtual>
      $<$<CXX_COMPILER_ID:Clang,GNU>:-Wpessimizing-move>
      $<$<CXX_COMPILER_ID:Clang,GNU>:-Wredundant-move>

      $<$<CXX_COMPILER_ID:GNU>:-Wno-missing-field-initializers>

      $<$<CXX_COMPILER_ID:MSVC>:/WX>
      $<$<CXX_COMPILER_ID:MSVC>:/W4>
      $<$<CXX_COMPILER_ID:MSVC>:/permissive->
      $<$<CXX_COMPILER_ID:MSVC>:/wd4324> # "structure was padded due to alignment specifier"

      # utils
      $<$<CXX_COMPILER_ID:Clang,GNU>:-std=c++20>
      $<$<CXX_COMPILER_ID:MSVC>:/std:c++latest>
  )

  target_compile_definitions(${TARGET} PRIVATE
    $<$<PLATFORM_ID:Windows>:PLATFORM_WINDOWS>
    $<$<PLATFORM_ID:Linux>:PLATFORM_LINUX>
  )

  target_compile_definitions(${TARGET} PRIVATE
    OUT_DIR="${CMAKE_BINARY_DIR}"
    TARGET_NAME="${TARGET}"
    $<$<PLATFORM_ID:Windows>:NOMINMAX>               # Disable min/max macros
    $<$<PLATFORM_ID:Windows>:NOCOMM>                 # Disable serial communication API
    $<$<PLATFORM_ID:Windows>:WIN32_LEAN_AND_MEAN>    # Disable some APIs
    $<$<PLATFORM_ID:Windows>:VC_EXTRALEAN>           # Disable even more APIs
    $<$<PLATFORM_ID:Windows>:UNICODE>                # Use unicode not ascii
  )
endfunction()


# Function to setup our targets
function(setup_app_target TARGET)
  # Parse arguments (<targetname> TESTS <test files>)
  # into args_TESTS
  set(options )
  set(oneValueArgs )
  set(multiValueArgs TESTS)
  cmake_parse_arguments(args "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN} )

  message(STATUS "Setting up application target ${TARGET}")

  # Setup include directories, public includes should be in <target>/include/<target>/public.h and private ones in <target>/src/private.h alongside .cpp files
  target_include_directories(${TARGET} PUBLIC include)
  target_include_directories(${TARGET} PRIVATE src)

  setup_app_target_options(${TARGET})

  # Add a test executable if the function was supplied test source files
  if (args_TESTS)
    add_executable(${TARGET}.tests ${args_TESTS})
    setup_app_target_options(${TARGET}.tests)
    target_link_libraries(${TARGET}.tests PRIVATE ${TARGET} Catch2::Catch2WithMain)
    target_compile_definitions(${TARGET}.tests PRIVATE RUN_TESTS)
    set_target_properties(${TARGET}.tests PROPERTIES FOLDER "tests")
    catch_discover_tests(${TARGET}.tests)
  endif()
endfunction()


# Add a shader target
function(SETUP_SHADERS TARGET)
  # Parse arguments (<targetname> SOURCES <sources> DEPENDENCIES <deps>)
  # into args_SOURCES and args_DEPENDENCIES
  set(options )
  set(oneValueArgs )
  set(multiValueArgs SOURCES DEPENDENCIES)
  cmake_parse_arguments(args "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN} )

  message(STATUS "Setting up shader target ${TARGET}.shaders")

  # Generate a list of include paths
  set(INCLUDE_ARGS )
  set(DEPENDS_PATHS )
  foreach(DEP IN LISTS args_DEPENDENCIES)
    list(APPEND INCLUDE_ARGS -I "${PROJECT_SOURCE_DIR}/libs/${DEP}/shaders/include/")
    list(APPEND INCLUDE_ARGS -I "${PROJECT_SOURCE_DIR}/apps/${DEP}/shaders/include/")
    list(APPEND DEPENDS_PATHS "${PROJECT_SOURCE_DIR}/libs/${DEP}/shaders/include/")
    list(APPEND DEPENDS_PATHS "${PROJECT_SOURCE_DIR}/apps/${DEP}/shaders/include/")
  endforeach()
  list(APPEND INCLUDE_ARGS -I "${PROJECT_SOURCE_DIR}/libs/${TARGET}/shaders/include/")
  list(APPEND INCLUDE_ARGS -I "${PROJECT_SOURCE_DIR}/apps/${TARGET}/shaders/include/")

  foreach(FILENAME IN LISTS args_SOURCES)
    set(GLSL_FILE ${FILENAME})
    cmake_path(ABSOLUTE_PATH GLSL_FILE)
    set(SPIRV_FILE "${PROJECT_BINARY_DIR}/${TARGET}/${FILENAME}.spv")

    add_custom_command(
      OUTPUT ${SPIRV_FILE}
      COMMAND glslc --target-env=vulkan1.2 -std=460 -g ${GLSL_FILE} -o ${SPIRV_FILE} ${INCLUDE_ARGS}
      DEPENDS ${GLSL_FILE}
  )
    list(APPEND SPIRV_FILES ${SPIRV_FILE})
  endforeach()

  add_custom_target(${TARGET}.shaders DEPENDS ${SPIRV_FILES})
  add_dependencies(${TARGET} ${TARGET}.shaders)
  set_target_properties(${TARGET}.shaders PROPERTIES FOLDER "shaders")
endfunction()


add_subdirectory(third_party)

# set(CMAKE_CXX_CLANG_TIDY clang-tidy -checks=-*,modernize-*,-modernize-avoid-c-arrays,-modernize-use-trailing-return-type,-modernize-return-braced-init-list,bugprone-*,-bugprone-easily-swappable-parameters,-bugprone-unchecked-optional-access,misc-*,-misc-no-recursion,-misc-non-private-member-variables-in-classes,performance-*,-performance-no-int-to-ptr)
add_subdirectory(libs/exo)
add_subdirectory(libs/cross)
add_subdirectory(libs/reflection)
add_subdirectory(libs/render)
add_subdirectory(libs/assets)
add_subdirectory(libs/gameplay)
add_subdirectory(libs/painter)
add_subdirectory(libs/engine)
add_subdirectory(libs/ui)
add_subdirectory(libs/ui_renderer)
add_subdirectory(libs/rhi)

# add_subdirectory(apps/editor)
# add_subdirectory(apps/render_sample)
# add_subdirectory(apps/biv)
# add_subdirectory(apps/test_iocp)
add_subdirectory(apps/handmade)
