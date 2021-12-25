set(GLSL_INCLUDES
  include/atmosphere.h
  include/bvh.h
  include/constants.h
  include/globals.h
  include/maths.h
  include/pbr.h
  include/raytracing.h
  include/types.h
  )

set(GLSL_FILES
  gui.frag.glsl
  gui.vert.glsl
  opaque.frag.glsl
  opaque.vert.glsl
  taa.comp.glsl
  tonemap.comp.glsl
  path_tracer.comp.glsl
  init_draw_calls.comp.glsl
  instances_culling.comp.glsl
  drawcalls_fill_predicate.comp.glsl
  scan/parallel_prefix_sum.comp.glsl
  scan/copy_instances_index.comp.glsl
  scan/copy_draw_calls.comp.glsl
  visibility_shading.comp.glsl
  )

foreach(GLSL_FILE IN LISTS GLSL_FILES)
  cmake_path(GET GLSL_FILE FILENAME SHADER_NAME)
  set(SPIRV_FILE "${PROJECT_BINARY_DIR}/shaders/${SHADER_NAME}.spv")

  add_custom_command(
    OUTPUT ${SPIRV_FILE}
    COMMAND glslc --target-env=vulkan1.2 -std=460 ${CMAKE_CURRENT_SOURCE_DIR}/${GLSL_FILE} -o ${SPIRV_FILE} -I ${CMAKE_CURRENT_SOURCE_DIR}/include
    DEPENDS ${GLSL_FILE} ${GLSL_INCLUDES}
    )


  list(APPEND SPIRV_FILES ${SPIRV_FILE})
endforeach()

add_custom_target(engine.shaders DEPENDS ${SPIRV_FILES})
install(FILES ${SPIRV_FILES} DESTINATION bin/shaders)
