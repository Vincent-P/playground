#include <GLFW/glfw3.h>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <glm/glm.hpp>
#include <imgui.h>
#include <iostream>
#include <string>
#include <thread>

#include "app.hpp"
#include "gltf.hpp"

int main(int /*argc*/, char ** argv)
{
    my_app::load_model(argv[1]);
    return 0;
}
