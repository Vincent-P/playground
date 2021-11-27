add_library(cross STATIC ${SOURCE_FILES})
setup_app_target(cross)

target_include_directories(cross SYSTEM PUBLIC include)
target_include_directories(cross PRIVATE src)
target_include_directories(cross SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/third_party ${CROSS_INCLUDES})
