#include "assets/subscene.h"

#include <exo/maths/numerics.h>
#include <exo/base/logger.h>

#include "schemas/exo_generated.h"
#include "schemas/subscene_generated.h"
#include "assets/flatbuffer_utils.h"

#include <flatbuffers/flatbuffers.h>


void SubScene::from_flatbuffer(const void *data, usize /*len*/)
{
    const auto *subscene_buffer = engine::schemas::GetSubScene(data);

    from_asset(subscene_buffer->asset(), this);

    const auto *subscene_transforms = subscene_buffer->transforms();
    const auto *transforms_data = reinterpret_cast<const float4x4*>(subscene_transforms->data());
    this->transforms = Vec<float4x4>(transforms_data, transforms_data + subscene_transforms->size());

    this->meshes = from<cross::UUID>(subscene_buffer->meshes());

    const auto *subscene_childrens = subscene_buffer->children();
    this->children.reserve(subscene_childrens->size());
    for (const auto *subscene_children : *subscene_childrens)
    {
        const auto *children_children = subscene_children->children();
        this->children.emplace_back(children_children->data(), children_children->data() + children_children->size());
    }

    const auto *subscene_roots = subscene_buffer->roots();
    for (auto subscene_root : *subscene_roots)
    {
        this->roots.push_back(subscene_root);
    }

    const auto *subscene_names = subscene_buffer->names();
    this->names.resize(this->transforms.size());
    for (u32 i_name = 0; i_name < subscene_names->size(); i_name += 1)
    {
        this->names[i_name] = std::string{subscene_names->Get(i_name)->c_str()};
    }
}

void SubScene::to_flatbuffer(flatbuffers::FlatBufferBuilder &builder, u32 &o_offset, u32 &o_size) const
{
    auto asset_offset = to_asset(this, builder);

    auto transforms_offset = builder.CreateVectorOfStructs(reinterpret_cast<const engine::schemas::exo::float4x4*>(transforms.data()), transforms.size());

    Vec<engine::schemas::exo::UUID> meshes_uuid = to<engine::schemas::exo::UUID>(this->meshes);
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

    auto names_offset = builder.CreateVectorOfStrings(names);

    auto roots_offset = builder.CreateVectorScalarCast<u32>(roots.data(), roots.size());

    engine::schemas::SubSceneBuilder subscene_builder{builder};
    subscene_builder.add_asset(asset_offset);
    subscene_builder.add_transforms(transforms_offset);
    subscene_builder.add_meshes(meshes_offset);
    subscene_builder.add_children(children_offset);
    subscene_builder.add_roots(roots_offset);
    subscene_builder.add_names(names_offset);
    auto subscene_offset = subscene_builder.Finish();

    // builder.Finish() doesn't add a file identifier
    engine::schemas::FinishSubSceneBuffer(builder, subscene_offset);

    o_offset = subscene_offset.o;
    o_size   = builder.GetSize();
}
