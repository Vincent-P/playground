set(SOURCE_FILES
  src/lib.cpp

  src/collections/pool.cpp
  src/collections/index_map.cpp

  src/memory/string_repository.cpp
  src/memory/free_list.cpp
  src/memory/buddy_allocator.cpp
  src/memory/linear_allocator.cpp
  src/memory/scope_stack.cpp

  src/maths/vectors.cpp
  src/maths/matrices.cpp

  src/cross/file_watcher.cpp
  src/cross/virtual_allocator.cpp
  src/cross/uuid.cpp
  )

if (WIN32)

  set(SOURCE_FILES
    ${SOURCE_FILES}
    src/cross/file_dialog_win32.cpp
    src/cross/utils_win32.cpp
    src/cross/window_win32.cpp
    src/cross/mapped_file_win32.cpp)

  set(CROSS_LIBS
    ${CROSS_LIBS}
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
    src/cross/file_dialog_linux.cpp
    src/cross/window_xcb.cpp
    src/cross/mapped_file_unix.cpp)

  set(CROSS_LIBS
          ${CROSS_LIBS}
          ${X11_xcb_LIB}
          ${X11_xkbcommon_LIB}
          ${X11_xkbcommon_X11_LIB})

  set(CROSS_INCLUDES
          ${CROSS_INCLUDES}
          ${X11_xcb_INCLUDE_PATH}
          ${X11_xkbcommon_INCLUDE_PATH}
          ${X11_xkbcommon_X11_INCLUDE_PATH})
endif()

add_library(exo STATIC ${SOURCE_FILES})
setup_app_target(exo)

target_link_libraries(exo PUBLIC ${CROSS_LIBS} fmt)

target_compile_options(exo PRIVATE ${APP_CXX_FLAGS})

target_include_directories(exo SYSTEM PUBLIC include)
target_include_directories(exo PRIVATE src)
target_include_directories(exo SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/third_party)

target_compile_definitions(exo PUBLIC
  $<$<BOOL:${WIN32}>:NOMINMAX>  # Disable min/max macros
  $<$<BOOL:${WIN32}>:NOCOMM> # Disable serial communication API
  $<$<BOOL:${WIN32}>:WIN32_LEAN_AND_MEAN> # Disable some APIs
  $<$<BOOL:${WIN32}>:VC_EXTRALEAN> # Disable even more APIs
  $<$<BOOL:${WIN32}>:UNICODE> # Use unicode not ascii
  )
