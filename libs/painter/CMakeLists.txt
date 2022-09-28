set(SOURCE_FILES
  include/painter/rect.h

  include/painter/font.h
  src/font.cpp

  include/painter/glyph_cache.h
  src/glyph_cache.cpp

  include/painter/painter.h
  src/painter.cpp

  include/painter/shelf_allocator.h
  src/shelf_allocator.cpp
)

add_library(painter STATIC ${SOURCE_FILES})
setup_app_target(painter)
target_link_libraries(painter PRIVATE
  exo
  harfbuzz
  freetype
)
