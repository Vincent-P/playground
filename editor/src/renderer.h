#pragma once
#include <exo/uuid.h>

#include <render/simple_renderer.h>

struct RenderWorld;
struct AssetManager;

struct RenderMesh
{
	Handle<vulkan::Buffer> index_buffer = {};
	Handle<vulkan::Buffer> positions_buffer = {};
	Handle<vulkan::Buffer> uvs_buffer       = {};
	Handle<vulkan::Buffer> submesh_buffer   = {};
	bool                   is_uploaded      = false;
	exo::UUID              mesh_asset       = {};
};

struct MeshRenderer
{
	exo::IndexMap         mesh_uuid_map;
	exo::Pool<RenderMesh> render_meshes;
};

struct Renderer
{
	SimpleRenderer base;
	MeshRenderer   mesh_renderer;
	AssetManager  *asset_manager = nullptr;

	static Renderer create(u64 window_handle, AssetManager *asset_manager);
	void            draw(const RenderWorld &world);
};
