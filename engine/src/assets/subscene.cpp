#include "assets/subscene.h"

#include <exo/maths/numerics.h>
#include <flatbuffers/flatbuffers.h>
#include "schemas/exo_generated.h"
#include "schemas/subscene_generated.h"

#include <exo/logger.h>


void SubScene::from_flatbuffer(const void *data, usize /*len*/)
{
    const auto *subscene_buffer = engine::schemas::GetSubScene(data);

    const auto *subscene_transforms = subscene_buffer->transforms();
    const auto *transforms_data = reinterpret_cast<const float4x4*>(subscene_transforms->data());
    this->transforms = Vec<float4x4>(transforms_data, transforms_data + subscene_transforms->size());

    const auto *subscene_meshes = subscene_buffer->meshes();
    this->meshes.reserve(subscene_meshes->size());
    for (const auto *subscene_mesh : *subscene_meshes)
    {
        ASSERT(subscene_mesh->v()->size() == 4);
        u32         values[4] = {};
        for (u32 i_value = 0; i_value < subscene_mesh->v()->size(); i_value += 1) {
            values[i_value] = subscene_mesh->v()->Get(i_value);
        }
        this->meshes.push_back(cross::UUID::from_values(values));
    }

    const auto *subscene_childrens = subscene_buffer->children();
    this->children.reserve(subscene_childrens->size());
    for (const auto *subscene_children : *subscene_childrens)
    {
        const auto *children = subscene_children->children();
        this->children.emplace_back(children->data(), children->data() + children->size());
    }

    for (const auto &mesh_uuid : this->meshes)
    {
        this->dependencies.push_back(mesh_uuid);
    }

    logger::info("[SubScene] read {} transforms from file.\n", subscene_buffer->transforms()->size());
}

void SubScene::to_flatbuffer(flatbuffers::FlatBufferBuilder &builder, u32 &o_offset, u32 &o_size) const
{
    auto transforms_offset = builder.CreateVectorOfStructs(reinterpret_cast<const engine::schemas::exo::float4x4*>(transforms.data()), transforms.size());

    Vec<engine::schemas::exo::UUID> meshes_uuid;
    meshes_uuid.reserve(this->meshes.size());
    for (const auto &mesh_uuid : this->meshes)
    {
        const auto *casted_uuid = reinterpret_cast<const engine::schemas::exo::UUID*>(&mesh_uuid);
        meshes_uuid.push_back(*casted_uuid);
    }
    auto meshes_offset = builder.CreateVectorOfStructs(meshes_uuid);

    Vec<flatbuffers::Offset<engine::schemas::EntityChildren>> entity_children_offsets;
    entity_children_offsets.reserve(this->children.size());
    for (const auto & entity_children : this->children)
    {
        auto entity_children_offset = engine::schemas::CreateEntityChildren(
            builder,
            builder.CreateVectorScalarCast<u32>(entity_children.data(), entity_children.size())
            );
        entity_children_offsets.push_back(entity_children_offset);
    }
    auto children_offset = builder.CreateVector(entity_children_offsets.data(), entity_children_offsets.size());

    engine::schemas::SubSceneBuilder subscene_builder{builder};
    subscene_builder.add_transforms(transforms_offset);
    subscene_builder.add_meshes(meshes_offset);
    subscene_builder.add_children(children_offset);
    auto subscene_offset = subscene_builder.Finish();

    // builder.Finish() doesn't add a file identifier
    engine::schemas::FinishSubSceneBuffer(builder, subscene_offset);

    logger::info("[SubScene] written {} transforms to file.\n", this->transforms.size());

    o_offset = subscene_offset.o;
    o_size   = builder.GetSize();
}
