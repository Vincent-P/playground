set(GLSL_INCLUDES
  include/atmosphere.h
  include/bvh.h
  include/constants.h
  include/csm.h
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
  tonemap.comp
  )

set(HLSL_VS_FILES
  # gui.vert.hlsl
  )

set(HLSL_PS_FILES
  # gui.frag.hlsl
  )

foreach(GLSL_FILE IN LISTS GLSL_FILES)
  get_filename_component(FILE_NAME ${GLSL_FILE} NAME)
  set(SPIRV_FILE "${PROJECT_BINARY_DIR}/shaders/${FILE_NAME}.spv")

  add_custom_command(
    OUTPUT ${SPIRV_FILE}
    COMMAND glslc --target-env=vulkan1.2 -std=460 ${CMAKE_CURRENT_SOURCE_DIR}/${GLSL_FILE} -o ${SPIRV_FILE} -I ${CMAKE_CURRENT_SOURCE_DIR}/include
    DEPENDS ${GLSL_FILE} ${GLSL_INCLUDES}
    )


  list(APPEND SPIRV_FILES ${SPIRV_FILE})
endforeach()

foreach(HLSL_FILE IN LISTS HLSL_VS_FILES)
  get_filename_component(FILE_NAME ${HLSL_FILE} NAME)
  set(SPIRV_FILE "${PROJECT_BINARY_DIR}/shaders/${FILE_NAME}.spv")

  add_custom_command(
    OUTPUT ${SPIRV_FILE}
    COMMAND dxc -spirv -E main  -T vs_6_7 ${CMAKE_CURRENT_SOURCE_DIR}/${HLSL_FILE} -Fo ${SPIRV_FILE}
    DEPENDS ${HLSL_FILE}
    )


  list(APPEND SPIRV_FILES ${SPIRV_FILE})
endforeach()

foreach(HLSL_FILE IN LISTS HLSL_PS_FILES)
  get_filename_component(FILE_NAME ${HLSL_FILE} NAME)
  set(SPIRV_FILE "${PROJECT_BINARY_DIR}/shaders/${FILE_NAME}.spv")

  add_custom_command(
    OUTPUT ${SPIRV_FILE}
    COMMAND dxc -spirv -E main  -T ps_6_7 ${CMAKE_CURRENT_SOURCE_DIR}/${HLSL_FILE} -Fo ${SPIRV_FILE}
    DEPENDS ${HLSL_FILE}
    )


  list(APPEND SPIRV_FILES ${SPIRV_FILE})
endforeach()


add_custom_target(shaders DEPENDS ${SPIRV_FILES})
install(FILES ${SPIRV_FILES} DESTINATION bin/shaders)
