set(SOURCE_FILES
  src/main.cpp
  src/app.cpp
)

add_executable(editor ${SOURCE_FILES})
setup_app_target(editor)
target_link_libraries(editor engine)
