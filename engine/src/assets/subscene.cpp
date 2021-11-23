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

    const auto *subscene_materials = subscene_buffer->materials();
    this->materials.reserve(subscene_materials->size());
    for (const auto *subscene_material : *subscene_materials)
    {
        ASSERT(subscene_material->v()->size() == 4);
        u32         values[4] = {};
        for (u32 i_value = 0; i_value < subscene_material->v()->size(); i_value += 1) {
            values[i_value] = subscene_material->v()->Get(i_value);
        }
        this->materials.push_back(cross::UUID::from_values(values));
    }
    // --

    for (const auto &mesh_uuid : this->meshes)
    {
        if (mesh_uuid.is_valid())
        {
            usize i_found = 0;
            for (; i_found < this->dependencies.size(); i_found += 1) {
                if (this->dependencies[i_found] == mesh_uuid) {
                    break;
                }
            }
            if (i_found >= this->dependencies.size())
            {
                this->dependencies.push_back(mesh_uuid);
            }
        }
    }

    for (auto material_uuid : this->materials)
    {
        this->dependencies.push_back(std::move(material_uuid));
    }
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

    auto names_offset = builder.CreateVectorOfStrings(names);

    auto roots_offset = builder.CreateVectorScalarCast<u32>(roots.data(), roots.size());

    Vec<engine::schemas::exo::UUID> materials_uuid;
    materials_uuid.reserve(this->materials.size());
    for (const auto &material_uuid : this->materials)
    {
        const auto *casted_uuid = reinterpret_cast<const engine::schemas::exo::UUID*>(&material_uuid);
        materials_uuid.push_back(*casted_uuid);
    }
    auto materials_offset = builder.CreateVectorOfStructs(materials_uuid);

    engine::schemas::SubSceneBuilder subscene_builder{builder};
    subscene_builder.add_transforms(transforms_offset);
    subscene_builder.add_meshes(meshes_offset);
    subscene_builder.add_children(children_offset);
    subscene_builder.add_roots(roots_offset);
    subscene_builder.add_names(names_offset);
    subscene_builder.add_materials(materials_offset);
    auto subscene_offset = subscene_builder.Finish();

    // builder.Finish() doesn't add a file identifier
    engine::schemas::FinishSubSceneBuffer(builder, subscene_offset);

    o_offset = subscene_offset.o;
    o_size   = builder.GetSize();
}
