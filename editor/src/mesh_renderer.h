#pragma once
#include <exo/collections/handle.h>
#include <exo/collections/index_map.h>
#include <exo/collections/pool.h>
#include <exo/collections/vector.h>
#include <exo/maths/matrices.h>
#include <exo/uuid.h>
#include <render/ring_buffer.h>
#include <render/vulkan/buffer.h>
#include <render/vulkan/pipelines.h>

struct RenderWorld;
struct AssetManager;
struct RenderGraph;
struct TextureDesc;
namespace vulkan
{
struct Device;
}

// -- Assets

struct RenderUploads
{
	Handle<vulkan::Buffer> dst_buffer    = {};
	usize                  dst_offset    = 0;
	usize                  upload_offset = 0;
	usize                  upload_size   = 0;
};

struct RenderImageUpload
{
	Handle<vulkan::Image> dst_image     = {};
	usize                 upload_offset = 0;
	usize                 upload_size   = 0;
	int3                  extent        = int3(1, 1, 1);
};

struct RenderTexture
{
	exo::UUID             texture_asset = {};
	Handle<vulkan::Image> image         = {};
	bool                  is_uploaded   = false;
};

struct RenderMaterial
{
	exo::UUID             material_asset             = {};
	Handle<RenderTexture> base_color_texture         = {};
	Handle<RenderTexture> normal_texture             = {};
	Handle<RenderTexture> metallic_roughness_texture = {};
	bool                  is_uploaded                = false;
};

struct RenderSubmesh
{
	Handle<RenderMaterial> material;
	u32                    index_count = 0;
	u32                    first_index = 0;
};

struct RenderMesh
{
	exo::UUID              mesh_asset       = {};
	Handle<vulkan::Buffer> index_buffer     = {};
	Handle<vulkan::Buffer> positions_buffer = {};
	Handle<vulkan::Buffer> uvs_buffer       = {};
	Handle<vulkan::Buffer> submesh_buffer   = {};
	Vec<RenderSubmesh>     render_submeshes = {};
	bool                   is_uploaded      = false;
};

// -- Draw

struct SimpleDraw
{
	u32                    instance_offset;
	u32                    instance_count;
	u32                    index_count;
	u32                    index_offset;
	Handle<vulkan::Buffer> index_buffer;
	u32                    i_submesh = 0;
};

struct MeshRenderer
{
	exo::IndexMap          mesh_uuid_map;
	exo::Pool<RenderMesh>  render_meshes;
	Handle<vulkan::Buffer> meshes_buffer;
	u32                    meshes_descriptor = u32_invalid;

	exo::IndexMap             material_uuid_map;
	exo::Pool<RenderMaterial> render_materials;
	Handle<vulkan::Buffer>    materials_buffer;
	u32                       materials_descriptor = u32_invalid;

	exo::IndexMap            texture_uuid_map;
	exo::Pool<RenderTexture> render_textures;

	RingBuffer instances_buffer;
	u32        instances_descriptor = u32_invalid;

	Handle<vulkan::GraphicsProgram> simple_program;

	// store intermediate result
	Vec<RenderUploads>     buffer_uploads;
	Vec<RenderImageUpload> image_uploads;
	Vec<SimpleDraw>        drawcalls;
	float4x4               projection = {};
	float4x4               view       = {};

	static MeshRenderer create(vulkan::Device &device);
};

void register_upload_nodes(RenderGraph &graph,
	MeshRenderer                       &mesh_renderer,
	vulkan::Device                     &device,
	RingBuffer                         &upload_buffer,
	AssetManager                       *asset_manager,
	const RenderWorld                  &world);

void register_graphics_nodes(RenderGraph &graph, MeshRenderer &mesh_renderer, Handle<TextureDesc> output);
