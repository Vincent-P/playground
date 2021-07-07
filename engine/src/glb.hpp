#pragma once

#include <filesystem>

#include "platform/mapped_file.hpp"
#include "base/vector.hpp"
#include "render/mesh.hpp"

namespace glb
{
    struct Scene
    {
        platform::MappedFile file;
        Vec<Mesh> meshes;
    };

    Scene load_file(const std::string_view &path);
};
