#pragma once
#include "ecs.h"
#include "gameplay/entity_world.h"
#include <exo/collections/pool.h>

#include "render/material.h"

class Inputs;
class AssetManager;

namespace UI {struct Context;}

class Scene
{
public:
    void init(AssetManager *_asset_manager, const Inputs *inputs);
    void destroy();

    void update(const Inputs &inputs);

    void display_ui(UI::Context &ui);

    AssetManager *asset_manager;
    EntityWorld entity_world;
    ECS::World world;
    Entity* main_camera_entity = nullptr;
    Vec<ECS::EntityId> meshes_entities;
};
