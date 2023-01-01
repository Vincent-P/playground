add_library(handmade_game SHARED src/game/lib.cpp)
setup_app_target(handmade_game)
target_link_libraries(handmade_game PRIVATE exo rhi)

# WIN32 specifies that it is a GUI exe using winmain
add_executable(handmade WIN32 src/main.cpp)
setup_app_target(handmade)
target_link_libraries(handmade PRIVATE exo)
target_compile_definitions(handmade PRIVATE GAME_DLL_PATH="$<TARGET_FILE:handmade_game>")
