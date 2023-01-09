set(SOURCE_FILES
  include/rhi/memory.h
  include/rhi/queues.h

  include/rhi/context.h
  src/context.cpp

  include/rhi/surface.h
  src/surface.cpp

  include/rhi/image.h
  src/image.cpp
)

add_library(rhi STATIC ${SOURCE_FILES})
setup_app_target(rhi)

target_link_libraries(rhi PUBLIC exo vulkan_headers)
target_compile_definitions(rhi PUBLIC VK_NO_PROTOTYPES)
target_link_libraries(rhi PRIVATE vma)
