#pragma once

#include <exo/option.h>
#include <cross/uuid.h>
#include "assets/asset.h"

// Hierarchy of entities made of meshes and transforms
struct SubScene : Asset
{
    Vec<float4x4> transforms;
    Vec<cross::UUID> meshes;
    Vec<Vec<u32>> children;

    const char *type_name() final { return "SubScene"; }
    void from_flatbuffer(const void *data, usize len) final;
    void to_flatbuffer(flatbuffers::FlatBufferBuilder &builder, u32 &o_offset, u32 &o_size) const final;
};
