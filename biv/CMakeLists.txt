set(SOURCE_FILES
  src/main.cpp
  src/glyph_cache.cpp
  src/painter.cpp
  src/ui.cpp
)

add_executable(biv ${SOURCE_FILES})
setup_app_target(biv)
target_link_libraries(biv
  PRIVATE
  engine
  # Luau.Compiler
  # Luau.VM
  harfbuzz
  freetype
  )

add_subdirectory(shaders/)
add_dependencies(biv biv.shaders)
