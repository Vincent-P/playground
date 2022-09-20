set(SOURCE_FILES
    include/ui/ui.h
    src/ui.cpp

    include/ui/docking.h
    src/docking.cpp

    include/ui/scroll.h
    src/scroll.cpp
)

add_library(ui STATIC ${SOURCE_FILES})
setup_app_target(ui)
target_link_libraries(ui PUBLIC exo painter cross)
