set(SOURCE_FILES
  src/main.cpp
  src/glyph_cache.cpp
  src/painter.cpp
  src/ui.cpp
  src/inputs.cpp
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

add_subdirectory(shaders/)
add_dependencies(biv biv.shaders)
