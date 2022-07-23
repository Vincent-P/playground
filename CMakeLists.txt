cmake_minimum_required(VERSION 3.20)

project(test-vulkan LANGUAGES C CXX)

# External dependencies
find_package(Threads)

add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Global C++ flags
add_compile_options(
  # colored output when using ninja generator
  $<$<CXX_COMPILER_ID:Clang,AppleClang>:-fcolor-diagnostics>
  $<$<CXX_COMPILER_ID:GNU>:-fdiagnostics-color=always>
  $<$<CXX_COMPILER_ID:Clang,AppleClang>:-fansi-escape-codes>

  # Disable warnings for external headers
  $<$<CXX_COMPILER_ID:MSVC>:/external:anglebrackets>
  $<$<CXX_COMPILER_ID:MSVC>:/external:W0>

  # Optimize for AVX2
  $<$<CXX_COMPILER_ID:MSVC>:/arch:AVX2>

  # Multi-processor compilation
  $<$<CXX_COMPILER_ID:MSVC>:/MP>
  )

# C++ flags for the project's code
# https://github.com/cpp-best-practices/cppbestpractices/blob/master/02-Use_the_Tools_Available.md#compilers
set(APP_CXX_FLAGS
  # zstd in ktx doesnt compile with this flag
  $<$<CXX_COMPILER_ID:Clang,AppleClang>:-march=native>

  # Warning level
  $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-Wall>
  $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-Wextra>
  $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-Wpedantic>
  $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-Wshadow>
  $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-Wconversion>
  $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-Wnon-virtual-dtor>
  $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-Woverloaded-virtual>

  $<$<CXX_COMPILER_ID:MSVC>:/W4>
  $<$<CXX_COMPILER_ID:MSVC>:/permissive-> # Enable standard conformance
  $<$<CXX_COMPILER_ID:MSVC>:/w14640> # Enable warning on thread un-safe static member initialization

  # utils
  # $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-fsanitize=address>
  # $<$<CXX_COMPILER_ID:MSVC>:/fsanitize=address>
  )

# C++ flags for third party libraries
set(THIRD_PARTY_CXX_FLAGS
  # Disable warnings for third-party libs
  $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-Wno-everything>
  $<$<CXX_COMPILER_ID:MSVC>:/W0>
  )

function(SETUP_APP_TARGET TARGET)
  message(STATUS "Setting up application target ${TARGET}")
  set_target_properties(${TARGET} PROPERTIES CXX_STANDARD 20)
  target_compile_options(${TARGET} PRIVATE ${APP_CXX_FLAGS})
  target_include_directories(${TARGET} SYSTEM PUBLIC include)
  target_include_directories(${TARGET} PRIVATE src)

  if (WIN32)
	target_compile_definitions(${TARGET} PRIVATE PLATFORM_WINDOWS)
  else()
	message( FATAL_ERROR "Platform not supported." )
  endif()
  target_compile_definitions(${TARGET} PRIVATE OUT_DIR="${CMAKE_BINARY_DIR}" TARGET_NAME="${TARGET}")
endfunction()

function(SETUP_THIRDPARTY_TARGET TARGET)
  message(STATUS "Setting up third party target ${TARGET}")
  set_target_properties(${TARGET} PROPERTIES CXX_STANDARD 17)
  set_target_properties(${TARGET} PROPERTIES FOLDER "third_party")
  target_compile_options(${TARGET} PRIVATE ${THIRD_PARTY_CXX_FLAGS})
endfunction()

function(SETUP_SHADERS TARGET)
  set(options )
  set(oneValueArgs )
  set(multiValueArgs SOURCES DEPENDENCIES)
  cmake_parse_arguments(args "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN} )

  message(STATUS "Adding target ${TARGET}.shaders")

  # Generate a list of include paths
  set(INCLUDE_ARGS )
  set(DEPENDS_PATHS )
  foreach(DEP IN LISTS args_DEPENDENCIES)
    list(APPEND INCLUDE_ARGS -I "${PROJECT_SOURCE_DIR}/${DEP}/shaders/include/")
    list(APPEND DEPENDS_PATHS "${PROJECT_SOURCE_DIR}/${DEP}/shaders/include/")
  endforeach()
  list(APPEND INCLUDE_ARGS -I "${PROJECT_SOURCE_DIR}/${TARGET}/shaders/include/")

  foreach(FILENAME IN LISTS args_SOURCES)
    set(SHADER_INPUT_DIR "${PROJECT_SOURCE_DIR}/${TARGET}/shaders")
    set(SHADER_OUTPUT_DIR "${PROJECT_BINARY_DIR}/${TARGET}/shaders")
    set(GLSL_FILE "${SHADER_INPUT_DIR}/${FILENAME}")
    set(SPIRV_FILE "${SHADER_OUTPUT_DIR}/${FILENAME}.spv")

    add_custom_command(
      OUTPUT ${SPIRV_FILE}
      COMMAND glslc --target-env=vulkan1.2 -std=460 ${GLSL_FILE} -o ${SPIRV_FILE} ${INCLUDE_ARGS}
      DEPENDS ${GLSL_FILE}
    )
    list(APPEND SPIRV_FILES ${SPIRV_FILE})
  endforeach()

  add_custom_target(${TARGET}.shaders DEPENDS ${SPIRV_FILES})
  add_dependencies(${TARGET} ${TARGET}.shaders)
endfunction()


add_subdirectory(third_party)

# set(CMAKE_CXX_CLANG_TIDY clang-tidy -checks=-*,modernize-*,-modernize-avoid-c-arrays,-modernize-use-trailing-return-type,-modernize-return-braced-init-list)
# set(CMAKE_CXX_CLANG_TIDY clang-tidy -checks=-*,bugprone-*,-bugprone-easily-swappable-parameters)
# set(CMAKE_CXX_CLANG_TIDY clang-tidy -checks=-*,misc-*)
# set(CMAKE_CXX_CLANG_TIDY clang-tidy -checks=-*,performance-*)

add_subdirectory(exo)
add_subdirectory(cross)
add_subdirectory(render)
# add_subdirectory(engine)
# add_subdirectory(editor)
add_subdirectory(render_sample)
add_subdirectory(biv)
