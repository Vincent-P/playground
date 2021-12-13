set(SOURCE_FILES
  src/lib.cpp
  src/serializer.cpp

  src/collections/pool.cpp
  src/collections/index_map.cpp

  src/memory/string_repository.cpp
  src/memory/free_list.cpp
  src/memory/buddy_allocator.cpp
  src/memory/linear_allocator.cpp
  src/memory/scope_stack.cpp
  src/memory/virtual_allocator.cpp

  src/maths/vectors.cpp
  src/maths/matrices.cpp

  src/os/file_watcher.cpp
  src/os/uuid.cpp
  )

if (WIN32)

  set(SOURCE_FILES
    ${SOURCE_FILES}
    src/os/file_dialog_win32.cpp
    src/os/utils_win32.cpp
    src/os/window_win32.cpp
    src/os/mapped_file_win32.cpp)

  set(OS_LIBS
    ${OS_LIBS}
    imm32
    rpcrt4 # for uuid
    )

elseif (UNIX)

  find_package(X11 REQUIRED)

  if (NOT X11_xkbcommon_INCLUDE_PATH)
    find_path(X11_xkbcommon_INCLUDE_PATH xkbcommon/xkbcommon.h /usr/include/libxkbcommon)
  endif()
  if (NOT X11_xkbcommon_X11_INCLUDE_PATH)
    find_path(X11_xkbcommon_X11_INCLUDE_PATH xkbcommon/xkbcommon-x11.h /usr/include/libxkbcommon)
  endif()

  set(SOURCE_FILES
    ${SOURCE_FILES}
    src/os/file_dialog_linux.cpp
    src/os/window_xcb.cpp
    src/os/mapped_file_unix.cpp)

  set(OS_LIBS
          ${OS_LIBS}
          ${X11_xcb_LIB}
          ${X11_xkbcommon_LIB}
          ${X11_xkbcommon_X11_LIB})

  set(OS_INCLUDES
          ${OS_INCLUDES}
          ${X11_xcb_INCLUDE_PATH}
          ${X11_xkbcommon_INCLUDE_PATH}
          ${X11_xkbcommon_X11_INCLUDE_PATH})
endif()

add_library(exo STATIC ${SOURCE_FILES})
setup_app_target(exo)

target_link_libraries(exo PUBLIC ${OS_LIBS} fmt parallel_hashmap tracy xxhash leaf)

target_compile_options(exo PRIVATE ${APP_CXX_FLAGS})

target_include_directories(exo SYSTEM PUBLIC include)
target_include_directories(exo PRIVATE src)

target_compile_definitions(exo PUBLIC
  $<$<BOOL:${WIN32}>:NOMINMAX>  # Disable min/max macros
  $<$<BOOL:${WIN32}>:NOCOMM> # Disable serial communication API
  $<$<BOOL:${WIN32}>:WIN32_LEAN_AND_MEAN> # Disable some APIs
  $<$<BOOL:${WIN32}>:VC_EXTRALEAN> # Disable even more APIs
  $<$<BOOL:${WIN32}>:UNICODE> # Use unicode not ascii
  )
