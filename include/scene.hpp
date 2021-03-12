#pragma once
#include "ecs.hpp"
#include "base/pool.hpp"
#include "gltf.hpp"

class Inputs;
namespace UI {struct Context;}

class Scene
{
public:
    void init();
    void destroy();

    void update(const Inputs &inputs);

    void display_ui(UI::Context &ui);

    ECS::World world;
    ECS::EntityId main_camera;

    Pool<gltf::Model> models;
};
