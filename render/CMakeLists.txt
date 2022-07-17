set(SOURCE_FILES

  # vulkan
  include/render/vulkan/operators.h
  include/render/vulkan/physical_device.h
  include/render/vulkan/queries.h
  include/render/vulkan/queues.h
  include/render/vulkan/synchronization.h
  include/render/vulkan/utils.h
  include/render/vulkan/buffer.h
  src/vulkan/buffer.cpp
  include/render/vulkan/commands.h
  src/vulkan/commands.cpp
  include/render/vulkan/context.h
  src/vulkan/context.cpp
  include/render/vulkan/descriptor_set.h
  src/vulkan/descriptor_set.cpp
  include/render/vulkan/device.h
  src/vulkan/device.cpp
  include/render/vulkan/image.h
  src/vulkan/image.cpp
  include/render/vulkan/pipelines.h
  src/vulkan/compute_pipeline.cpp
  src/vulkan/graphics_pipeline.cpp
  include/render/vulkan/surface.h
  src/vulkan/surface.cpp
  include/render/vulkan/shader.h
  src/vulkan/shader.cpp
  include/render/vulkan/framebuffer.h
  src/vulkan/framebuffer.cpp

  # render graph
  include/render/render_graph/resource_registry.h
  src/render_graph/resource_registry.cpp
  include/render/render_graph/graph.h
  src/render_graph/graph.cpp
  include/render/render_graph/builtins.h
  src/render_graph/builtins.cpp

  include/render/ring_buffer.h
  src/ring_buffer.cpp

  include/render/shader_watcher.h

  include/render/simple_renderer.h
  src/simple_renderer.cpp
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
