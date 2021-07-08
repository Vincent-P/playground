set(SOURCE_FILES
  src/asset_manager.cpp
  src/main.cpp
  src/app.cpp
  src/camera.cpp
  src/ecs.cpp
  src/scene.cpp
  src/gltf.cpp
  src/glb.cpp
  src/inputs.cpp
  src/tools.cpp
  src/ui.cpp
  src/base/pool.cpp
  src/base/vectors.cpp
  src/base/free_list.cpp
  src/platform/file_watcher.cpp
  src/platform/mapped_file.cpp
  # src/render/bvh.cpp
  src/render/renderer.cpp
  src/render/gpu_pool.cpp
  src/render/ring_buffer.cpp

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

if (WIN32)

  set(SOURCE_FILES
    ${SOURCE_FILES}
    src/platform/file_dialog_win32.cpp
    src/platform/utils_win32.cpp
    src/platform/window_win32.cpp)

  set(ENGINE_LIBS
    ${ENGINE_LIBS}
    imm32)

elseif (UNIX)

  set(SOURCE_FILES
    ${SOURCE_FILES}
    src/platform/window_xcb.cpp)

else()

endif()

add_executable(engine ${SOURCE_FILES})
set_target_properties(engine PROPERTIES CXX_STANDARD 20)
target_compile_options(engine PRIVATE ${APP_CXX_FLAGS})

target_include_directories(engine PRIVATE src)
target_include_directories(engine SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/third_party)

target_link_libraries(engine
  ${ENGINE_LIBS}
  imgui
  simdjson
  fmt
  stb_image
  meshopt
  vma
  Vulkan::Vulkan
  Threads::Threads)


target_compile_definitions(engine PRIVATE
  $<$<BOOL:${WIN32}>:NOMINMAX>  # Disable min/max macros
  $<$<BOOL:${WIN32}>:NOCOMM> # Disable serial communication API
  $<$<BOOL:${WIN32}>:WIN32_LEAN_AND_MEAN> # Disable some APIs
  $<$<BOOL:${WIN32}>:VC_EXTRALEAN> # Disable even more APIs
  $<$<BOOL:${WIN32}>:UNICODE> # Use unicode not ascii
  $<$<BOOL:${WIN32}>:VK_USE_PLATFORM_WIN32_KHR>
  )

add_subdirectory(shaders/)
add_dependencies(engine shaders)
