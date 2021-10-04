#pragma once

#include <filesystem>

#include <cross/mapped_file.h>
#include <exo/collections/vector.h>
#include "assets/mesh.h"
#include "assets/material.h"
#include "assets/texture.h"

namespace glb
{

    struct MeshInstance
    {
        u32 i_mesh;
        float4x4 transform;
    };

    struct Scene
    {
        cross::MappedFile file;
        Vec<Mesh> meshes;
        Vec<MeshInstance> instances;
        Vec<Material> materials;
        Vec<Texture> textures;
    };

    Scene load_file(const std::string_view &path);
};
