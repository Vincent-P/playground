#pragma once
#include <exo/collections/pool.h>

#include "render/material.h"
#include "render/mesh.h"

#include <filesystem>

namespace UI { struct Context; }

struct Texture
{
    std::string name;
};

class AssetManager
{
public:
    void load_texture(const std::filesystem::path &path);
    void choose_texture(Handle<Texture> &selected);
    void choose_mesh(Handle<Mesh> &selected);
    void display_ui(UI::Context &ui);

    Pool<Texture> textures;
    Vec<Mesh> meshes;
};
