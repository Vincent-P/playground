set(SOURCE_FILES
  include/engine/render_world.h
  include/engine/render_world_system.h
  include/engine/scene.h
  include/engine/camera.h
  src/camera.cpp
  src/render_world_system.cpp
  src/scene.cpp
)

add_library(engine STATIC ${SOURCE_FILES})
setup_app_target(engine)
target_link_libraries(engine PUBLIC
  exo
  assets
  render
  gameplay
  )

target_link_libraries(engine PRIVATE ui)
