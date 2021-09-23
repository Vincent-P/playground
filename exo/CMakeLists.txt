set(SOURCE_FILES
  src/lib.cpp
  src/pool.cpp
  src/free_list.cpp
  src/buddy_allocator.cpp
  src/maths/vectors.cpp
  src/maths/matrices.cpp
  )

add_library(exo STATIC ${SOURCE_FILES})
setup_app_target(exo)

target_compile_options(exo PRIVATE ${APP_CXX_FLAGS})

target_include_directories(exo SYSTEM PUBLIC include)
target_include_directories(exo PRIVATE src)
target_include_directories(exo SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/third_party)

target_link_libraries(exo PUBLIC fmt)
