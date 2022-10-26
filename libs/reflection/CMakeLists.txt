set(SOURCE_FILES
    include/reflection/reflection.h
    include/reflection/reflection_serializer.h
    src/reflection.cpp
)

add_library(reflection STATIC ${SOURCE_FILES})
setup_app_target(reflection)
target_link_libraries(reflection PUBLIC exo)
