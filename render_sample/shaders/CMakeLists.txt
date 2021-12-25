set(GLSL_INCLUDES
  ${PROJECT_SOURCE_DIR}/engine/shaders/include/atmosphere.h
  ${PROJECT_SOURCE_DIR}/engine/shaders/include/bvh.h
  ${PROJECT_SOURCE_DIR}/engine/shaders/include/constants.h
  ${PROJECT_SOURCE_DIR}/engine/shaders/include/globals.h
  ${PROJECT_SOURCE_DIR}/engine/shaders/include/maths.h
  ${PROJECT_SOURCE_DIR}/engine/shaders/include/pbr.h
  ${PROJECT_SOURCE_DIR}/engine/shaders/include/raytracing.h
  ${PROJECT_SOURCE_DIR}/engine/shaders/include/types.h
  )

set(GLSL_FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/font.frag.glsl
  ${CMAKE_CURRENT_SOURCE_DIR}/font.vert.glsl
  )

foreach(GLSL_FILE IN LISTS GLSL_FILES)
  cmake_path(GET GLSL_FILE FILENAME SHADER_NAME)
  set(SPIRV_FILE "${PROJECT_BINARY_DIR}/shaders/${SHADER_NAME}.spv")

  add_custom_command(
    OUTPUT ${SPIRV_FILE}
    COMMAND glslc --target-env=vulkan1.2 -std=460 ${GLSL_FILE} -o ${SPIRV_FILE} -I ${PROJECT_SOURCE_DIR}/engine/shaders/include -I ${PROJECT_SOURCE_DIR}/render_sample/shaders/include
    DEPENDS ${GLSL_FILE} ${GLSL_INCLUDES}
    )


  list(APPEND SPIRV_FILES ${SPIRV_FILE})
endforeach()

add_custom_target(render_sample.shaders DEPENDS ${SPIRV_FILES})
install(FILES ${SPIRV_FILES} DESTINATION bin/shaders)
