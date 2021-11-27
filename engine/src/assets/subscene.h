#pragma once

#include <exo/base/option.h>
#include <exo/cross/uuid.h>
#include "assets/asset.h"

// Hierarchy of entities made of meshes and transforms
struct SubScene : Asset
{
    Vec<u32> roots;

    // SoA nodes layout
    Vec<float4x4> transforms;
    Vec<cross::UUID> meshes;
    Vec<std::string> names;
    Vec<Vec<u32>> children;

    const char *type_name() const final { return "SubScene"; }
    void from_flatbuffer(const void *data, usize len) final;
    void to_flatbuffer(flatbuffers::FlatBufferBuilder &builder, u32 &o_offset, u32 &o_size) const final;
};
