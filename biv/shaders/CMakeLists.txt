set(GLSL_INCLUDES
  ${PROJECT_SOURCE_DIR}/engine/shaders/include/base/constants.h
  ${PROJECT_SOURCE_DIR}/engine/shaders/include/base/maths.h
  ${PROJECT_SOURCE_DIR}/engine/shaders/include/base/types.h
  ${PROJECT_SOURCE_DIR}/engine/shaders/include/base/bindless.h
  ${PROJECT_SOURCE_DIR}/biv/shaders/include/biv/globals.h
  )

set(GLSL_FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/ui.frag.glsl
  ${CMAKE_CURRENT_SOURCE_DIR}/ui.vert.glsl
  ${CMAKE_CURRENT_SOURCE_DIR}/viewer.frag.glsl
  ${CMAKE_CURRENT_SOURCE_DIR}/viewer.vert.glsl
  )

foreach(GLSL_FILE IN LISTS GLSL_FILES)
  cmake_path(GET GLSL_FILE FILENAME SHADER_NAME)
  set(SPIRV_FILE "${PROJECT_BINARY_DIR}/shaders/${SHADER_NAME}.spv")

  add_custom_command(
    OUTPUT ${SPIRV_FILE}
    COMMAND glslc --target-env=vulkan1.2 -std=460 ${GLSL_FILE} -o ${SPIRV_FILE} -I ${PROJECT_SOURCE_DIR}/engine/shaders/include -I ${PROJECT_SOURCE_DIR}/biv/shaders/include
    DEPENDS ${GLSL_FILE} ${GLSL_INCLUDES}
    )


  list(APPEND SPIRV_FILES ${SPIRV_FILE})
endforeach()

add_custom_target(biv.shaders DEPENDS ${SPIRV_FILES})
install(FILES ${SPIRV_FILES} DESTINATION bin/shaders)
