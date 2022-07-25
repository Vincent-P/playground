// -*- mode: glsl; -*-
#ifndef _EDITOR_MESH_H
#define _EDITOR_MESH_H
#include "base/bindless.h"
#include "base/types.h"

struct SubmeshDescriptor
{
	u32 i_material;
	u32 first_index;
	u32 first_vertex;
	u32 index_count;
};

struct MeshDescriptor
{
	u32 index_buffer_descriptor;
	u32 positions_buffer_descriptor;
	u32 uvs_buffer_descriptor;
	u32 submesh_buffer_descriptor;
};

struct InstanceDescriptor
{
	float4x4 transform;
	u32      i_mesh_descriptor;
	u32      padding0;
	u32      padding1;
	u32      padding2;
};

struct MaterialDescriptor
{
	float4 base_color_factor;
	float4 emissive_factor;
	float metallic_factor;
	float roughness_factor;
	u32 base_color_texture;
	u32 normal_texture;
	u32 metallic_roughness_texture;
	float rotation;
	float2 offset;
	float2 scale;
	float2 pad00;
};

#define BINDLESS_BUFFER layout(set = GLOBAL_BINDLESS_SET, binding = GLOBAL_BUFFER_BINDING) buffer

BINDLESS_BUFFER PositionsBuffer { float4 positions[]; } global_buffers_positions[];
BINDLESS_BUFFER UvsBuffer { float2 uvs[]; } global_buffers_uvs[];
BINDLESS_BUFFER IndexBuffers { u32 indices[]; } global_buffers_indices[];
BINDLESS_BUFFER SubmeshBuffer { SubmeshDescriptor submeshes[]; } global_buffers_submeshes[];
BINDLESS_BUFFER MeshBuffer { MeshDescriptor meshes[]; } global_buffers_meshes[];
BINDLESS_BUFFER InstanceBuffer { InstanceDescriptor instances[]; } global_buffers_instances[];
BINDLESS_BUFFER MaterialBuffer { MaterialDescriptor materials[]; } global_buffers_materials[];

#endif
