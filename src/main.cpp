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


int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    my_app::App app;
    app.run();
    return 0;
}
