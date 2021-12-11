set(SOURCE_FILES
  # random
  src/main.cpp
  src/app.cpp
  src/camera.cpp
  src/scene.cpp
  src/inputs.cpp
  src/ui.cpp

  # assets
  src/assets/asset_manager.cpp
  src/assets/texture.cpp
  src/assets/importers/gltf_importer.cpp
  src/assets/importers/png_importer.cpp
  src/assets/importers/ktx2_importer.cpp
  src/assets/material.cpp
  src/assets/mesh.cpp
  src/assets/subscene.cpp

  # gameplay framework
  src/gameplay/component.cpp
  src/gameplay/entity.cpp
  src/gameplay/entity_world.cpp
  src/gameplay/loading_context.cpp

  # gameplay
  src/gameplay/components/camera_component.cpp
  src/gameplay/systems/editor_camera_systems.cpp

  # rendering api
  src/render/render_world_system.cpp
  src/render/renderer.cpp
  src/render/render_timings.cpp
  src/render/base_renderer.cpp
  src/render/bvh.cpp
  src/render/ring_buffer.cpp
  src/render/streamer.cpp
  src/render/unified_buffer_storage.cpp

  # rendering framework
  src/render/vulkan/context.cpp
  src/render/vulkan/commands.cpp
  src/render/vulkan/descriptor_set.cpp
  src/render/vulkan/bindless_set.cpp
  src/render/vulkan/image.cpp
  src/render/vulkan/buffer.cpp
  src/render/vulkan/device.cpp
  src/render/vulkan/shader.cpp
  src/render/vulkan/surface.cpp
  src/render/vulkan/graphics_pipeline.cpp
  src/render/vulkan/compute_pipeline.cpp)

add_executable(engine ${SOURCE_FILES})
setup_app_target(engine)
install(TARGETS engine RUNTIME)

target_precompile_headers(engine PRIVATE src/pch.h)

target_compile_options(engine PRIVATE ${APP_CXX_FLAGS})

target_include_directories(engine PRIVATE src)

target_link_libraries(engine
  exo
  Vulkan::Vulkan
  Threads::Threads
  imgui
  meshopt
  vma
  libktx
  libspng
  tracy
  rapidjson
  meow_hash)

add_subdirectory(shaders/)
add_dependencies(engine shaders)

target_compile_definitions(engine PRIVATE
  ASSET_PATH="${CMAKE_SOURCE_DIR}/assets"
  COMPILED_ASSET_PATH="${CMAKE_SOURCE_DIR}/compiled_assets"

  # Vulkan platform
  $<$<BOOL:${WIN32}>:VK_USE_PLATFORM_WIN32_KHR>
  $<$<BOOL:${UNIX}>:VK_USE_PLATFORM_XCB_KHR>
  )
