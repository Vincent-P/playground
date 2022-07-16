set(SOURCE_FILES
  src/main.cpp
)

set(DEPENDENCIES
  exo
  render
  )

add_executable(render_sample ${SOURCE_FILES})
setup_app_target(render_sample)
target_link_libraries(render_sample PRIVATE ${DEPENDENCIES})
