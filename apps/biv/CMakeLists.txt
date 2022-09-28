set(SOURCE_FILES
  src/inputs.cpp
  src/inputs.h
  src/main.cpp
)

add_executable(biv ${SOURCE_FILES})
setup_app_target(biv)
target_link_libraries(biv
  PRIVATE
  exo
  cross
  render
  ui
  ui_renderer
  painter
  libspng
  )

setup_shaders(biv
  SOURCES
  shaders/viewer.frag.glsl
  shaders/viewer.vert.glsl
  DEPENDENCIES
  engine
  ui_renderer
)
