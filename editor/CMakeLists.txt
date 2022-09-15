set(SOURCE_FILES
  src/app.cpp
  src/app.h
  src/main.cpp
  src/renderer.cpp
  src/renderer.h
  src/mesh_renderer.cpp
  src/mesh_renderer.h
  src/custom_ui.cpp
  src/custom_ui.h
)

add_executable(editor ${SOURCE_FILES})
setup_app_target(editor)
target_link_libraries(editor PUBLIC engine painter ui ui_renderer sokol)


setup_shaders(editor
  SOURCES
  simple_mesh.frag.glsl
  simple_mesh.vert.glsl
  srgb_pass.comp.glsl
  DEPENDENCIES
  engine
)
