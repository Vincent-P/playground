#pragma once

#include <exo/option.h>
#include <exo/os/uuid.h>
#include "assets/asset.h"

// Hierarchy of entities made of meshes and transforms
struct SubScene : Asset
{
    static Asset *create();
    const char *type_name() const final { return "SubScene"; }
    void serialize(exo::Serializer& serializer) final;
    void display_ui() final {}

    // --
    Vec<u32> roots;

    // SoA nodes layout
    Vec<float4x4> transforms;
    Vec<os::UUID> meshes;
    Vec<const char*> names;
    Vec<Vec<u32>> children;
};

template<>
void exo::Serializer::serialize<SubScene>(SubScene &data);
