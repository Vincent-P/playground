#pragma once
#include "ecs.hpp"
#include "base/pool.hpp"
#include "gltf.hpp"

#include "render/material.hpp"

class Inputs;
namespace UI {struct Context;}

struct Image
{
    Vec<u8> data;
    bool srgb;
};

class Scene
{
public:
    void init();
    void destroy();

    void import_model(Handle<gltf::Model> model_handle);
    void update(const Inputs &inputs);

    void display_ui(UI::Context &ui);

    ECS::World world;
    ECS::EntityId main_camera;
    Vec<ECS::EntityId> meshes_entities;

    Vec<Material> materials;
    Pool<Mesh> meshes;
    Vec<Vertex> vertices;
    Vec<u32> indices;
    Vec<Image> images;

    Pool<gltf::Model> models;
};
