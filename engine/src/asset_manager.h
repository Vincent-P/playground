#pragma once
#include <exo/collections/pool.h>

#include "render/material.h"
#include "render/mesh.h"
#include "render/texture.h"

#include <filesystem>

namespace UI { struct Context; }

class AssetManager
{
public:
    Vec<Texture> textures;
    Vec<Mesh> meshes;
    Vec<Material> materials;
};
