#pragma once

#include <exo/base/option.h>
#include <exo/cross/uuid.h>
#include "assets/asset.h"

// Hierarchy of entities made of meshes and transforms
struct SubScene : Asset
{
    const char *type_name() const final { return "SubScene"; }
    void serialize(Serializer& serializer) final;
    void display_ui() final {}

    // --
    Vec<u32> roots;

    // SoA nodes layout
    Vec<float4x4> transforms;
    Vec<cross::UUID> meshes;
    Vec<const char*> names;
    Vec<Vec<u32>> children;
};

template<>
void Serializer::serialize<SubScene>(SubScene &data);
