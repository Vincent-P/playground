#pragma once
#include <exo/collections/pool.h>

#include "assets/material.h"
#include "assets/mesh.h"
#include "assets/texture.h"

#include <filesystem>

namespace UI { struct Context; }

class AssetManager
{
public:
    Vec<Texture> textures;
    Vec<Mesh> meshes;
    Vec<Material> materials;
};
