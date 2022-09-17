set(SOURCE_FILES
  include/exo/collections/array.h
  include/exo/collections/dynamic_array.h
  include/exo/collections/enum_array.h
  include/exo/collections/handle.h
  include/exo/collections/handle_map.h
  include/exo/collections/index_map.h
  include/exo/collections/index_map_serializer.h
  include/exo/collections/iterator_facade.h
  include/exo/collections/map.h
  include/exo/collections/pool.h
  include/exo/collections/pool_serializer.h
  include/exo/collections/set.h
  include/exo/collections/vector.h
  include/exo/hash.h
  include/exo/logger.h
  include/exo/macros/assert.h
  include/exo/macros/compiler.h
  include/exo/macros/defer.h
  include/exo/macros/packed.h
  include/exo/maths/aabb.h
  include/exo/maths/matrices.h
  include/exo/maths/numerics.h
  include/exo/maths/pointer.h
  include/exo/maths/quaternion.h
  include/exo/maths/vectors.h
  include/exo/maths/vectors_swizzle.h
  include/exo/maths.h
  include/exo/memory/linear_allocator.h
  include/exo/memory/scope_stack.h
  include/exo/memory/string_repository.h
  include/exo/memory/virtual_allocator.h
  include/exo/option.h
  include/exo/result.h
  include/exo/serializer.h

  include/exo/uuid.h
  include/exo/uuid_formatter.h
  include/exo/uuid_serializer.h
  src/collections/index_map.cpp
  src/collections/index_map_serializer.cpp
  src/collections/pool.cpp
  src/collections/pool_serializer.cpp

  src/maths/matrices.cpp
  src/maths/vectors.cpp
  src/memory/linear_allocator.cpp
  src/memory/scope_stack.cpp
  src/memory/string_repository.cpp
  src/memory/virtual_allocator.cpp
  src/serializer.cpp
  src/uuid.cpp
  src/uuid_serializer.cpp

  include/exo/path.h
  src/path.cpp

  engine.natvis
  phmap.natvis
   "include/exo/string_serializer.h" "include/exo/serializer_helper.h" "src/macros/assert.cpp")

set(TEST_FILES
  src/path.cpp
  src/path.tests.cpp
)

add_library(exo STATIC ${SOURCE_FILES})
setup_app_target(exo TESTS ${TEST_FILES})
target_link_libraries(exo PUBLIC fmt parallel_hashmap tracy xxhash)
