set(SCHEMA_FILES
  exo.fbs
  asset.fbs
  mesh.fbs
  subscene.fbs
  )

foreach(FBS_FILE IN LISTS SCHEMA_FILES)
  cmake_path(GET FBS_FILE STEM FILE_NAME)
  set(GENERATED_FILE "${CMAKE_CURRENT_SOURCE_DIR}/generated/${FILE_NAME}_generated.h")

  add_custom_command(
    OUTPUT ${GENERATED_FILE}
    COMMAND flatc --cpp -o generated/schemas --include-prefix "schemas/" ${FBS_FILE}
    DEPENDS ${FBS_FILE}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

  list(APPEND GENERATED_FILES ${GENERATED_FILE})
endforeach()

add_custom_target(schemas DEPENDS ${GENERATED_FILES})
