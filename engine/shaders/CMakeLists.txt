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
  gui.frag
  gui.vert
  opaque.frag
  opaque.vert
  taa.comp
  tonemap.comp
  path_tracer.comp
  init_draw_calls.comp
  instances_culling.comp
  drawcalls_fill_predicate.comp
  scan/parallel_prefix_sum.comp
  scan/copy_instances_index.comp
  scan/copy_draw_calls.comp
  visibility_shading.comp
  )

foreach(GLSL_FILE IN LISTS GLSL_FILES)
  cmake_path(GET GLSL_FILE FILENAME SHADER_NAME)
  set(SPIRV_FILE "${PROJECT_BINARY_DIR}/shaders/${SHADER_NAME}.spv")

  add_custom_command(
    OUTPUT ${SPIRV_FILE}
    COMMAND glslc --target-env=vulkan1.2 -O -std=460 ${CMAKE_CURRENT_SOURCE_DIR}/${GLSL_FILE} -o ${SPIRV_FILE} -I ${CMAKE_CURRENT_SOURCE_DIR}/include
    DEPENDS ${GLSL_FILE} ${GLSL_INCLUDES}
    )


  list(APPEND SPIRV_FILES ${SPIRV_FILE})
endforeach()

add_custom_target(shaders DEPENDS ${SPIRV_FILES})
install(FILES ${SPIRV_FILES} DESTINATION bin/shaders)
