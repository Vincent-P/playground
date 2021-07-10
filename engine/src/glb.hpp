#pragma once

#include <filesystem>

#include "platform/mapped_file.hpp"
#include "base/vector.hpp"
#include "render/mesh.hpp"

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
