set(SOURCE_FILES
    include/ui/ui.h
    src/ui.cpp
)

add_library(ui STATIC ${SOURCE_FILES})
setup_app_target(ui)
target_link_libraries(ui PRIVATE exo painter cross)
