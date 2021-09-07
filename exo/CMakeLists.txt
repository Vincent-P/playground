set(SOURCE_FILES
  src/lib.cpp
  src/pool.cpp
  src/vectors.cpp
  src/matrices.cpp
  src/free_list.cpp
  src/buddy_allocator.cpp
  )

add_library(exo STATIC ${SOURCE_FILES})

set_target_properties(exo PROPERTIES CXX_STANDARD 20)
target_compile_options(exo PRIVATE ${APP_CXX_FLAGS})

target_include_directories(exo SYSTEM PUBLIC include)
target_include_directories(exo PRIVATE src)
target_include_directories(exo SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/third_party)

target_link_libraries(exo PUBLIC fmt)
