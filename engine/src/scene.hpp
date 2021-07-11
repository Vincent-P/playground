#pragma once
#include "ecs.hpp"
#include "base/pool.hpp"

#include "render/material.hpp"

class Inputs;
class AssetManager;

namespace UI {struct Context;}

class Scene
{
public:
    void init(AssetManager *_asset_manager);
    void destroy();

    void update(const Inputs &inputs);

    void display_ui(UI::Context &ui);

    AssetManager *asset_manager;
    ECS::World world;
    ECS::EntityId main_camera;
    Vec<ECS::EntityId> meshes_entities;
};
