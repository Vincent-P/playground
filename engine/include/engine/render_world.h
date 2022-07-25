#pragma once
#include <exo/collections/map.h>
#include <exo/collections/vector.h>
#include <exo/maths/aabb.h>
#include <exo/maths/matrices.h>
#include <exo/uuid.h>

struct DrawableInstance
{
	exo::UUID mesh_asset;
	float4x4  world_transform;
	exo::AABB world_bounds;
};

struct MeshInstance
{
	// BVHNode bvh_root = {};
	Vec<u32> instances;
	Vec<u32> materials;
	u32      first_instance = 0;
};

// Description of the world that the renderer will use
struct RenderWorld
{
	// input
	float4x4 main_camera_view;
	float4x4 main_camera_view_inverse;
	float4x4 main_camera_projection;
	float4x4 main_camera_projection_inverse;

	Vec<DrawableInstance> drawable_instances;
};
