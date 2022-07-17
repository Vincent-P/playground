set(SOURCE_FILES
  include/exo/collections/array.h
  include/exo/collections/dynamic_array.h
  include/exo/collections/enum_array.h
  include/exo/collections/handle.h
  include/exo/collections/index_map.h
  include/exo/collections/iterator_facade.h
  include/exo/collections/map.h
  include/exo/collections/pool.h
  include/exo/collections/set.h
  include/exo/collections/vector.h
  src/collections/index_map.cpp
  src/collections/pool.cpp

  include/exo/macros/assert.h
  include/exo/macros/compiler.h
  include/exo/macros/defer.h
  include/exo/macros/packed.h

  include/exo/memory/buddy_allocator.h
  include/exo/memory/free_list.h
  include/exo/memory/linear_allocator.h
  include/exo/memory/scope_stack.h
  include/exo/memory/string_repository.h
  include/exo/memory/virtual_allocator.h
  src/memory/buddy_allocator.cpp
  src/memory/free_list.cpp
  src/memory/linear_allocator.cpp
  src/memory/scope_stack.cpp
  src/memory/string_repository.cpp
  src/memory/virtual_allocator.cpp

  include/exo/maths/aabb.h
  include/exo/maths/matrices.h
  include/exo/maths/numerics.h
  include/exo/maths/pointer.h
  include/exo/maths/quaternion.h
  include/exo/maths/vectors.h
  include/exo/maths/vectors_swizzle.h
  include/exo/maths.h
  src/maths/matrices.cpp
  src/maths/vectors.cpp

  include/exo/hash.h
  include/exo/logger.h
  include/exo/option.h
  include/exo/result.h
  include/exo/serializer.h
  include/exo/time.h
  include/exo/uuid.h

  src/lib.cpp
  src/serializer.cpp
  src/uuid.cpp

  engine.natvis
  phmap.natvis
  )

add_library(exo STATIC ${SOURCE_FILES})
setup_app_target(exo)
target_link_libraries(exo PUBLIC fmt parallel_hashmap tracy xxhash)
target_compile_definitions(exo PUBLIC
  $<$<BOOL:${WIN32}>:NOMINMAX>  # Disable min/max macros
  $<$<BOOL:${WIN32}>:NOCOMM> # Disable serial communication API
  $<$<BOOL:${WIN32}>:WIN32_LEAN_AND_MEAN> # Disable some APIs
  $<$<BOOL:${WIN32}>:VC_EXTRALEAN> # Disable even more APIs
  $<$<BOOL:${WIN32}>:UNICODE> # Use unicode not ascii
  )
