set(SOURCE_FILES
  include/assets/asset.h
  include/assets/asset_constructors.h
  include/assets/asset_manager.h
  include/assets/importers/generic_importer.h
  include/assets/importers/gltf_importer.h
  include/assets/importers/ktx2_importer.h
  include/assets/importers/png_importer.h
  include/assets/material.h
  include/assets/mesh.h
  include/assets/subscene.h
  include/assets/texture.h
  src/asset.cpp
  src/asset_manager.cpp
  src/importers/gltf_importer.cpp
  src/importers/ktx2_importer.cpp
  src/importers/png_importer.cpp
  src/material.cpp
  src/mesh.cpp
  src/subscene.cpp
  src/texture.cpp
)

add_library(assets STATIC ${SOURCE_FILES})
setup_app_target(assets)
target_link_libraries(assets PUBLIC exo cross rapidjson)
target_link_libraries(assets PRIVATE libspng libktx meow_hash)
target_compile_definitions(assets PUBLIC
  ASSET_PATH="${CMAKE_SOURCE_DIR}/data/assets"
  COMPILED_ASSET_PATH="${CMAKE_SOURCE_DIR}/data/compiled_assets"
)
