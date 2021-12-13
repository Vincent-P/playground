#include "assets/material.h"
#include "assets/asset_constructors.h"

#include <imgui.h>

static int material_ctor = global_asset_constructors().add_constructor("MTRL", &Material::create);

Asset *Material::create()
{
    return new Material();
}

void Material::display_ui()
{
    ImGui::SliderFloat4("base color factor", this->base_color_factor.data(), 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat4("emissive factor", this->emissive_factor.data(), 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("metallic factor", &this->metallic_factor, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("roughness factor", &this->roughness_factor, 0.0f, 1.0f, "%.2f");
}

void Material::serialize(exo::Serializer& serializer)
{
    serializer.serialize(*this);
}

template <>
void exo::Serializer::serialize<Material>(Material &data)
{
    const char *id = "MTRL";
    serialize(id);
    serialize(static_cast<Asset &>(data));
    serialize(data.base_color_factor);
    serialize(data.emissive_factor);
    serialize(data.metallic_factor);
    serialize(data.roughness_factor);
    serialize(data.base_color_texture);
    serialize(data.normal_texture);
    serialize(data.metallic_roughness_texture);
    serialize(data.uv_transform.offset);
    serialize(data.uv_transform.scale);
    serialize(data.uv_transform.rotation);
}
