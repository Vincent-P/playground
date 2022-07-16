set(SOURCE_FILES
  include/render/vulkan/buffer.h
  include/render/vulkan/commands.h
  include/render/vulkan/context.h
  include/render/vulkan/descriptor_set.h
  include/render/vulkan/device.h
  include/render/vulkan/image.h
  include/render/vulkan/operators.h
  include/render/vulkan/physical_device.h
  include/render/vulkan/pipelines.h
  include/render/vulkan/queries.h
  include/render/vulkan/queues.h
  include/render/vulkan/surface.h
  include/render/vulkan/synchronization.h
  include/render/vulkan/utils.h
  src/vulkan/buffer.cpp
  src/vulkan/commands.cpp
  src/vulkan/compute_pipeline.cpp
  src/vulkan/context.cpp
  src/vulkan/descriptor_set.cpp
  src/vulkan/device.cpp
  src/vulkan/graphics_pipeline.cpp
  src/vulkan/image.cpp
  src/vulkan/shader.cpp
  src/vulkan/surface.cpp

  include/render/base_renderer.h
  include/render/bvh.h
  include/render/render_timings.h
  include/render/ring_buffer.h
  include/render/streamer.h
  include/render/unified_buffer_storage.h
  src/base_renderer.cpp
  src/bvh.cpp
  src/render_timings.cpp
  src/ring_buffer.cpp
  src/streamer.cpp
  src/unified_buffer_storage.cpp

# render/include/render/renderer.h
# render/include/render/render_world.h
# render/include/render/render_world_system.h
# render/src/renderer.cpp
# render/src/render_world_system.cpp
)

add_library(render STATIC ${SOURCE_FILES})
setup_app_target(render)

target_link_libraries(render PUBLIC exo cross volk vma)
target_compile_definitions(render PUBLIC
  $<$<BOOL:${WIN32}>:NOMINMAX>  # Disable min/max macros
  $<$<BOOL:${WIN32}>:NOCOMM> # Disable serial communication API
  $<$<BOOL:${WIN32}>:WIN32_LEAN_AND_MEAN> # Disable some APIs
  $<$<BOOL:${WIN32}>:VC_EXTRALEAN> # Disable even more APIs
  $<$<BOOL:${WIN32}>:UNICODE> # Use unicode not ascii
  )
