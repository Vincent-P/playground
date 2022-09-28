set(SOURCE_FILES
  src/main.cpp
)

set(DEPENDENCIES
  exo
  cross
  )

add_executable(test_iocp ${SOURCE_FILES})
setup_app_target(test_iocp)
target_link_libraries(test_iocp PRIVATE ${DEPENDENCIES})
