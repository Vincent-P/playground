set(SOURCE_FILES
  include/ui_renderer/ui_renderer.h
  src/ui_renderer.cpp
)

add_library(ui_renderer STATIC ${SOURCE_FILES})
setup_app_target(ui_renderer)

target_link_libraries(ui_renderer PUBLIC painter render)

setup_shaders(ui_renderer
  SOURCES
  shaders/ui.frag.glsl
  shaders/ui.vert.glsl
  DEPENDENCIES
  engine
)
