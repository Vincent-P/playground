set(SOURCE_FILES
  src/main.cpp
  src/glyph_cache.cpp
)

add_executable(render_sample ${SOURCE_FILES})
setup_app_target(render_sample)
target_compile_options(render_sample PRIVATE ${APP_CXX_FLAGS})
target_include_directories(render_sample PRIVATE src)
target_link_libraries(render_sample
  PRIVATE
  engine
  # Luau.Compiler
  # Luau.VM
  harfbuzz
  freetype
  )

add_subdirectory(shaders/)
add_dependencies(render_sample render_sample.shaders)
