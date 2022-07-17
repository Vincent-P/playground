set(SOURCE_FILES
  src/main.cpp
  src/glyph_cache.cpp
  src/painter.cpp
  src/ui.cpp
  src/inputs.cpp

  src/font.h
  src/font.cpp

  src/shelf_allocator.h
  src/shelf_allocator.cpp
)

add_executable(biv ${SOURCE_FILES})
setup_app_target(biv)
target_link_libraries(biv
  PRIVATE
  exo
  cross
  render
  harfbuzz
  freetype
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
