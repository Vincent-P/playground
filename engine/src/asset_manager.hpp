#pragma once
#include "base/pool.hpp"

#include "render/material.hpp"
#include "render/mesh.hpp"

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
