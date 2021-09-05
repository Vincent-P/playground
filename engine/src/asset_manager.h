#pragma once
#include <exo/collections/pool.h>

#include "render/material.h"
#include "render/mesh.h"

#include <filesystem>

namespace UI { struct Context; }

struct Texture
{
    void *ktx_texture;
};

class AssetManager
{
public:
    Vec<Texture> textures;
    Vec<Mesh> meshes;
    Vec<Material> materials;
};
