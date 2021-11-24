#include "assets/material.h"

#include <exo/collections/vector.h>

#include "schemas/material_generated.h"
#include "assets/flatbuffer_utils.h"

#include <imgui/imgui.h>

void Material::from_flatbuffer(const void *data, usize /*len*/)
{
    ASSERT(engine::schemas::MaterialBufferHasIdentifier(data));
    const auto *material_buffer = engine::schemas::GetMaterial(data);

    this->base_color_factor          = from(material_buffer->base_color_factor());
    this->emissive_factor            = from(material_buffer->emissive_factor());
    this->metallic_factor            = material_buffer->metallic_factor();
    this->roughness_factor           = material_buffer->roughness_factor();
    this->base_color_texture         = from(material_buffer->base_color_texture());
    this->normal_texture             = from(material_buffer->normal_texture());
    this->metallic_roughness_texture = from(material_buffer->metallic_roughness_texture());
    this->uv_transform               = *reinterpret_cast<const TextureTransform*>(material_buffer->uv_transform());
}

void Material::to_flatbuffer(flatbuffers::FlatBufferBuilder &builder, u32 &o_offset, u32 &o_size) const
{
    engine::schemas::MaterialBuilder material_builder{builder};
    material_builder.add_base_color_factor(to(this->base_color_factor));
    material_builder.add_emissive_factor(to(this->emissive_factor));
    material_builder.add_metallic_factor(this->metallic_factor);
    material_builder.add_roughness_factor(this->roughness_factor);
    material_builder.add_base_color_texture(to(this->base_color_texture));
    material_builder.add_normal_texture(to(this->normal_texture));
    material_builder.add_metallic_roughness_texture(to(this->metallic_roughness_texture));
    material_builder.add_uv_transform(reinterpret_cast<const engine::schemas::TextureTransform*>(&this->uv_transform));

    // builder.Finish() doesn't add a file identifier
    auto material_offset = material_builder.Finish();

    engine::schemas::FinishMaterialBuffer(builder, material_offset);
    o_offset = material_offset.o;
    o_size   = builder.GetSize();
}

void Material::display_ui()
{
    ImGui::SliderFloat4("base color factor", this->base_color_factor.data(), 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat4("emissive factor", this->emissive_factor.data(), 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("metallic factor", &this->metallic_factor, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("roughness factor", &this->roughness_factor, 0.0f, 1.0f, "%.2f");
}
