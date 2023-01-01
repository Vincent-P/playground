set(SOURCE_FILES
  include/rhi/memory.h
  include/rhi/operators.h
  include/rhi/physical_device.h
  include/rhi/queries.h
  include/rhi/queues.h
  include/rhi/synchronization.h
  include/rhi/buffer.h
  src/buffer.cpp
  include/rhi/commands.h
  src/commands.cpp
  include/rhi/context.h
  src/context.cpp
  include/rhi/descriptor_set.h
  src/descriptor_set.cpp
  include/rhi/device.h
  src/device.cpp
  include/rhi/image.h
  src/image.cpp
  include/rhi/pipelines.h
  src/compute_pipeline.cpp
  src/graphics_pipeline.cpp
  include/rhi/surface.h
  src/surface.cpp
  include/rhi/shader.h
  src/shader.cpp
  include/rhi/framebuffer.h
  src/framebuffer.cpp
  include/rhi/utils.h
  src/utils.cpp
)

add_library(rhi STATIC ${SOURCE_FILES})
setup_app_target(rhi)

target_link_libraries(rhi PUBLIC exo cross volk)
target_link_libraries(rhi PRIVATE vma)
