#pragma once
#include "gameplay/entity_world.h"
#include <exo/collections/pool.h>

class Inputs;
struct AssetManager;
struct Mesh;
struct SubScene;

struct Scene
{
public:
    void init(AssetManager *_asset_manager, const Inputs *inputs);
    void destroy();
    void update(const Inputs &inputs);
    void display_ui();

    void import_mesh(Mesh *mesh);
    Entity* import_subscene_rec(const SubScene *subscene, u32 i_node);
    void import_subscene(SubScene *subscene);

    AssetManager *asset_manager;
    EntityWorld entity_world;
    Entity* main_camera_entity = nullptr;
};
