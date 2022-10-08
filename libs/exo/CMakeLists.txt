set(SOURCE_FILES
  include/exo/collections/array.h
  include/exo/collections/dynamic_array.h
  include/exo/collections/enum_array.h
  include/exo/collections/handle.h
  include/exo/collections/handle_map.h

  include/exo/collections/index_map.h
  src/collections/index_map.cpp
  include/exo/collections/index_map_serializer.h
  src/collections/index_map_serializer.cpp

  include/exo/collections/iterator_facade.h
  include/exo/collections/map.h
  include/exo/collections/pool.h
  src/collections/pool.cpp
  include/exo/collections/pool_serializer.h
  src/collections/pool_serializer.cpp
  include/exo/collections/set.h
  include/exo/collections/vector.h
  include/exo/collections/span.h

  include/exo/macros/assert.h
  src/macros/assert.cpp
  include/exo/macros/compiler.h
  include/exo/macros/defer.h
  include/exo/macros/packed.h

  include/exo/maths.h
  include/exo/maths/aabb.h
  include/exo/maths/matrices.h
  src/maths/matrices.cpp
  include/exo/maths/numerics.h
  include/exo/maths/u128.h
  include/exo/maths/pointer.h
  include/exo/maths/quaternion.h
  include/exo/maths/vectors.h
  src/maths/vectors.cpp
  include/exo/maths/vectors_swizzle.h

  include/exo/memory/linear_allocator.h
  src/memory/linear_allocator.cpp
  include/exo/memory/scope_stack.h
  src/memory/scope_stack.cpp
  include/exo/memory/string_repository.h
  src/memory/string_repository.cpp
  include/exo/memory/virtual_allocator.h
  src/memory/virtual_allocator.cpp
  include/exo/memory/dynamic_buffer.h
  src/memory/dynamic_buffer.cpp

  include/exo/option.h
  include/exo/result.h

  include/exo/serializer.h
  src/serializer.cpp

  include/exo/string_serializer.h
  include/exo/serializer_helper.h

  include/exo/uuid.h
  src/uuid.cpp
  include/exo/uuid_formatter.h
  include/exo/uuid_serializer.h
  src/uuid_serializer.cpp

  include/exo/path.h
  src/path.cpp

  include/exo/profile.h

  include/exo/hash.h
  include/exo/logger.h

  include/exo/forward_container.h

  engine.natvis
  phmap.natvis
)

set(TEST_FILES
  tests/path.cpp
  tests/pool.cpp
)

add_library(exo STATIC ${SOURCE_FILES})
setup_app_target(exo TESTS ${TEST_FILES})
target_link_libraries(exo PUBLIC fmt parallel_hashmap tracy xxhash)
