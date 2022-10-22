set(SOURCE_FILES
  include/gameplay/component.h
  include/gameplay/components/camera_component.h
  include/gameplay/components/mesh_component.h
  include/gameplay/entity.h
  include/gameplay/entity_world.h
  include/gameplay/input_bindings.def
  include/gameplay/inputs.h
  include/gameplay/loading_context.h
  include/gameplay/system.h
  include/gameplay/system_registry.h
  include/gameplay/systems/editor_camera_systems.h
  include/gameplay/update_context.h
  include/gameplay/update_stages.h
  src/component.cpp
  src/components/camera_component.cpp
  src/components/mesh_component.cpp
  src/entity.cpp
  src/entity_world.cpp
  src/inputs.cpp
  src/loading_context.cpp
  src/systems/editor_camera_systems.cpp
)

add_library(gameplay STATIC ${SOURCE_FILES})
setup_app_target(gameplay)
target_link_libraries(gameplay PUBLIC exo assets reflection)
target_link_libraries(gameplay PRIVATE ui)
