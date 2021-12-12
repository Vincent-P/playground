set(SOURCE_FILES
  src/main.cpp
  src/app.cpp
)

add_executable(editor ${SOURCE_FILES})
setup_app_target(editor)
target_compile_options(editor PRIVATE ${APP_CXX_FLAGS})
target_include_directories(editor PRIVATE src)
target_link_libraries(editor engine)
