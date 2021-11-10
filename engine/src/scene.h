#pragma once
#include "gameplay/entity_world.h"
#include <exo/collections/pool.h>

class Inputs;
struct AssetManager;
struct Mesh;
struct SubScene;

namespace UI {struct Context;}

class Scene
{
public:
    void init(AssetManager *_asset_manager, const Inputs *inputs);
    void destroy();
    void update(const Inputs &inputs);
    void display_ui(UI::Context &ui);

    void import_mesh(Mesh *mesh);
    void import_subscene(SubScene *subscene);

    AssetManager *asset_manager;
    EntityWorld entity_world;
    Entity* main_camera_entity = nullptr;
};
