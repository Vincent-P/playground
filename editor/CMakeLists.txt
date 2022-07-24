set(SOURCE_FILES
  src/app.cpp
  src/app.h
  src/main.cpp
  src/renderer.cpp
  src/renderer.h
)

add_executable(editor ${SOURCE_FILES})
setup_app_target(editor)
target_link_libraries(editor PUBLIC engine)
