set(SOURCE_FILES
  src/file_watcher.cpp
  src/uuid.cpp
  )

if (WIN32)

  set(SOURCE_FILES
    ${SOURCE_FILES}
    src/file_dialog_win32.cpp
    src/utils_win32.cpp
    src/window_win32.cpp
    src/mapped_file_win32.cpp)

  set(CROSS_LIBS
    ${CROSS_LIBS}
    imm32)

elseif (UNIX)

  set(SOURCE_FILES
    ${SOURCE_FILES}
    src/file_dialog_linux.cpp
    src/window_xcb.cpp
    src/mapped_file_unix.cpp)

  set(CROSS_LIBS
          ${CROSS_LIBS}
          xcb
          xkbcommon
          xkbcommon-x11)

endif()


add_library(cross STATIC ${SOURCE_FILES})
setup_app_target(cross)

target_link_libraries(cross PUBLIC ${CROSS_LIBS})
target_link_libraries(cross PRIVATE exo fmt)

target_compile_options(cross PRIVATE ${APP_CXX_FLAGS})

target_include_directories(cross SYSTEM PUBLIC include)
target_include_directories(cross PRIVATE src)
target_include_directories(cross SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/third_party)

target_compile_definitions(cross PUBLIC
  $<$<BOOL:${WIN32}>:NOMINMAX>  # Disable min/max macros
  $<$<BOOL:${WIN32}>:NOCOMM> # Disable serial communication API
  $<$<BOOL:${WIN32}>:WIN32_LEAN_AND_MEAN> # Disable some APIs
  $<$<BOOL:${WIN32}>:VC_EXTRALEAN> # Disable even more APIs
  $<$<BOOL:${WIN32}>:UNICODE> # Use unicode not ascii
  $<$<BOOL:${WIN32}>:VK_USE_PLATFORM_WIN32_KHR>

  $<$<BOOL:${UNIX}>:VK_USE_PLATFORM_XCB_KHR>
  )
