#pragma once

#include <filesystem>

#include <cross/mapped_file.h>
#include <exo/collections/vector.h>
#include "render/mesh.h"

namespace glb
{

    struct MeshInstance
    {
        u32 i_mesh;
        float4x4 transform;
    };

    struct Scene
    {
        platform::MappedFile file;
        Vec<Mesh> meshes;
        Vec<MeshInstance> instances;
    };

    Scene load_file(const std::string_view &path);
};
