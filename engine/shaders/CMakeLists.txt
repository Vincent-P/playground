set(GLSL_INCLUDES
  include/atmosphere.h
  include/bvh.h
  include/constants.h
  include/csm.h
  include/globals.h
  include/imgui.h
  include/maths.h
  include/pbr.h
  include/raytracing.h
  include/types.h
  include/voxels.h
  )

set(GLSL_FILES
  gui.frag
  gui.vert
  tonemap.comp.glsl
  average_luminance_histo.comp
  build_luminance_histo.comp
  opaque.vert
  opaque.frag
  opaque_prepass.frag
  path_tracer.comp.glsl
  hybrid.frag
  taa.comp.glsl
  )

foreach(GLSL_FILE ${GLSL_FILES})
  get_filename_component(FILE_NAME ${GLSL_FILE} NAME)
  set(SPIRV_FILE "${PROJECT_BINARY_DIR}/${FILE_NAME}.spv")

  add_custom_command(
    OUTPUT ${SPIRV_FILE}
    COMMAND glslc --target-env=vulkan1.2 -std=460 ${CMAKE_CURRENT_SOURCE_DIR}/${GLSL_FILE} -o ${SPIRV_FILE} -I ${CMAKE_CURRENT_SOURCE_DIR}/include
    DEPENDS ${GLSL_FILE} ${GLSL_INCLUDES}
    )

  list(APPEND SPIRV_FILES ${SPIRV_FILE})
endforeach(GLSL_FILE)


add_custom_target(shaders DEPENDS ${SPIRV_FILES})
