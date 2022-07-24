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
  painter
  libspng
  )

setup_shaders(biv
  SOURCES
  ui.frag.glsl
  ui.vert.glsl
  viewer.frag.glsl
  viewer.vert.glsl
  DEPENDENCIES
  engine
)
