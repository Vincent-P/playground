# Common code used by both projects
add_library(handmade_common src/common.cpp)

add_library(handmade_game SHARED src/game/lib.cpp)
setup_app_target(handmade_game)
target_link_libraries(handmade_game PRIVATE exo rhi handmade_common)

# WIN32 specifies that it is a GUI exe using winmain
add_executable(handmade WIN32 src/main.cpp)
setup_app_target(handmade)
target_link_libraries(handmade PRIVATE handmade_common)
target_compile_definitions(handmade PRIVATE GAME_DLL_PATH="$<TARGET_FILE:handmade_game>")
