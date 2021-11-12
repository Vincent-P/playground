#include "assets/mesh.h"

#include <flatbuffers/flatbuffers.h>
#include "schemas/exo_generated.h"
#include "schemas/mesh_generated.h"

#include <exo/logger.h>


void Mesh::from_flatbuffer(const void *data, usize /*len*/)
{
    ASSERT(engine::schemas::MeshBufferHasIdentifier(data));
    auto mesh_buffer = engine::schemas::GetMesh(data);

    const auto *mesh_indices = mesh_buffer->indices();
    const auto *indices_data = reinterpret_cast<const u32*>(mesh_indices->data());
    this->indices = Vec<u32>(indices_data, indices_data + mesh_indices->size());

    const auto *mesh_positions = mesh_buffer->positions();
    const auto *positions_data = reinterpret_cast<const float4*>(mesh_positions->data());
    this->positions = Vec<float4>(positions_data, positions_data + mesh_positions->size());

    const auto *mesh_uvs = mesh_buffer->uvs();
    const auto *uvs_data = reinterpret_cast<const float2*>(mesh_uvs->data());
    this->uvs = Vec<float2>(uvs_data, uvs_data + mesh_uvs->size());

    const auto *mesh_submeshes = mesh_buffer->submeshes();
    this->submeshes.reserve(mesh_submeshes->size());
    for (const auto *mesh_submesh : *mesh_submeshes)
    {
        this->submeshes.push_back({
            .first_index  = mesh_submesh->first_index(),
            .first_vertex = mesh_submesh->first_vertex(),
            .index_count  = mesh_submesh->index_count(),
            .material     = {},
        });
    }
}

template <typename T>
static auto create_vector_of_struct(auto &builder, const auto &vector)
{
    return builder.CreateVectorOfStructs(reinterpret_cast<const T*>(vector.data()), vector.size());
}

void Mesh::to_flatbuffer(flatbuffers::FlatBufferBuilder &builder, u32 &o_offset, u32 &o_size) const
{
    auto indices_offset = builder.CreateVectorScalarCast<u32>(indices.data(), indices.size());
    auto positions_offset = create_vector_of_struct<engine::schemas::exo::float4>(builder, this->positions);
    auto uvs_offset = create_vector_of_struct<engine::schemas::exo::float2>(builder, this->uvs);

    Vec<flatbuffers::Offset<engine::schemas::SubMesh>> submeshes_offsets;
    submeshes_offsets.resize(this->submeshes.size());
    engine::schemas::SubMeshBuilder submesh_builder{builder};
    for (usize i_submesh = 0; i_submesh < this->submeshes.size(); i_submesh += 1)
    {
        const auto &submesh = this->submeshes[i_submesh];

        submesh_builder.add_first_index(submesh.first_index);
        submesh_builder.add_first_vertex(submesh.first_vertex);
        submesh_builder.add_index_count(submesh.index_count);

        const auto uuid = engine::schemas::exo::UUID{flatbuffers::span<const u32, 4>(submesh.material.data)};
        submesh_builder.add_material(&uuid);

        submeshes_offsets[i_submesh] = submesh_builder.Finish();
    }
    auto submeshes_offset = builder.CreateVector(submeshes_offsets);


    engine::schemas::MeshBuilder mesh_builder{builder};
    mesh_builder.add_indices(indices_offset);
    mesh_builder.add_positions(positions_offset);
    mesh_builder.add_uvs(uvs_offset);
    mesh_builder.add_submeshes(submeshes_offset);
    auto mesh_offset = mesh_builder.Finish();

    // builder.Finish() doesn't add a file identifier
    engine::schemas::FinishMeshBuffer(builder, mesh_offset);

    o_offset = mesh_offset.o;
    o_size   = builder.GetSize();
}
