# include <iostream>

# define GLM_FORCE_RADIANS
# define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

# include "model.hpp"
# include "tiny_gltf.h"

namespace my_app
{
    void Mesh::draw(vk::CommandBuffer& cmd) const
    {
        for (const auto& primitive: primitives_)
        {
            cmd.drawIndexed(primitive.index_count, 1, primitive.first_index, primitive.first_vertex, 0);
        }
    }

    Model::Model(std::string path)
    {
        std::string err, warn;
        if (!loader.LoadASCIIFromFile(&model_, &err, &warn, path)
            || !err.empty()
            || !warn.empty())
        {
            if (!err.empty())
                std::clog << err;
            if (!warn.empty())
                std::clog << warn;
            throw std::runtime_error("Failed to load model.");
        }

        LoadMeshesBuffers();
    }

    void Model::LoadMeshesBuffers()
    {
        for (const auto& mesh: model_.meshes)
        {
            Mesh m;

            for (const auto& primitive: mesh.primitives)
            {
                std::uint32_t first_vertex = vertices_.size();
                std::uint32_t first_index  = indices_.size();
                std::uint32_t index_count = 0;

                const auto& attrs = primitive.attributes;
                auto position = attrs.find("POSITION");
                if (position == attrs.end())
                    throw std::runtime_error("The mesh doesn't have vertex positions.");

                {
                    const auto& position_acc  = model_.accessors[position->second];
                    const auto& position_view = model_.bufferViews[position_acc.bufferView];
                    auto total_offset = position_acc.byteOffset + position_view.byteOffset;
                    auto vertices = reinterpret_cast<float*>(&(model_.buffers[position_view.buffer].data[total_offset]));

                    for (size_t i = 0; i < position_acc.count; i++)
                    {
                        Vertex vertex;
                        vertex.pos = glm::make_vec3(&vertices[i * 3]);
                        vertices_.push_back(std::move(vertex));
                    }
                }

                {
                    const auto& indices_acc  = model_.accessors[primitive.indices];
                    const auto& indices_view = model_.bufferViews[indices_acc.bufferView];
                    auto total_offset = indices_acc.byteOffset + indices_view.byteOffset;
                    index_count = indices_acc.count;

                    switch (indices_acc.componentType)
                    {
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                    {
                        auto indices = reinterpret_cast<uint16_t*>(&(model_.buffers[indices_view.buffer].data[total_offset]));
                        for (size_t i = 0; i < indices_acc.count; i++)
                        {
                            indices_.push_back(indices[i]);
                        }

                    } break;
                    default:
                        break;
                    }
                }

                Primitive p = {first_vertex, first_index, index_count};
                m.primitives_.push_back(std::move(p));
            }

            meshes_.push_back(std::move(m));
        }
    }

    void Model::draw(vk::CommandBuffer& cmd) const
    {
        for (const auto& mesh: meshes_)
        {
            mesh.draw(cmd);
        }
    }
}
