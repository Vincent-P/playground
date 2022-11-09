set(SOURCE_FILES
  include/cross/platform.h
  include/cross/file_dialog.h
  include/cross/window.h
  include/cross/mapped_file.h
  include/cross/file_watcher.h
  include/cross/events.h
  include/cross/buttons.h
  include/cross/keyboard_keys.def
  src/file_watcher.cpp

  include/cross/jobmanager.h
  include/cross/jobs/job.h
  include/cross/jobs/waitable.h
  include/cross/jobs/custom.h
  include/cross/jobs/foreach.h
  include/cross/jobs/readfiles.h
  )

if (WIN32)
  set(SOURCE_FILES
	${SOURCE_FILES}
	src/file_dialog_win32.cpp
	src/platform_win32.cpp
	src/mapped_file_win32.cpp
	src/utils_win32.cpp
	src/utils_win32.h
	src/window_win32.cpp

	src/jobmanager_win32.h
	src/jobmanager_win32.cpp
	src/jobs/job_win32.h
	src/jobs/waitable_win32.cpp
	src/jobs/readfiles_win32.cpp
      )

elseif (UNIX)
  # Find X11 and xkbcommon
  find_package(X11 REQUIRED)
  if (NOT X11_xkbcommon_INCLUDE_PATH)
	find_path(X11_xkbcommon_INCLUDE_PATH xkbcommon/xkbcommon.h /usr/include/libxkbcommon)
  endif()
  if (NOT X11_xkbcommon_X11_INCLUDE_PATH)
	find_path(X11_xkbcommon_X11_INCLUDE_PATH xkbcommon/xkbcommon-x11.h /usr/include/libxkbcommon)
  endif()

  set(SOURCE_FILES
	${SOURCE_FILES}
	src/os/window_xcb.cpp
	src/os/mapped_file_unix.cpp
	src/os/file_dialog_linux.cpp)

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

add_library(cross STATIC ${SOURCE_FILES})
setup_app_target(cross)
target_link_libraries(cross PRIVATE ${OS_LIBS} exo)
