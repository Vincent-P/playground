#include "renderer.h"

#include <exo/collections/index_map.h>
#include <exo/collections/pool.h>
#include <exo/uuid_formatter.h>

#include <assets/asset_manager.h>
#include <assets/mesh.h>
#include <engine/render_world.h>
#include <render/vulkan/buffer.h>
#include <render/vulkan/device.h>

#include <bit>

template <typename T> Handle<T> as_handle(u64 bytes)
{
	static_assert(sizeof(Handle<T>) == sizeof(u64));
	return std::bit_cast<Handle<T>>(bytes);
}

template <typename T> u64 to_u64(Handle<T> handle)
{
	static_assert(sizeof(Handle<T>) == sizeof(u64));
	return std::bit_cast<u64>(handle);
}

struct SubmeshDescriptor
{
	u32 i_material   = u32_invalid;
	u32 first_index  = u32_invalid;
	u32 first_vertex = u32_invalid;
	u32 index_count  = u32_invalid;
};

struct MeshDescriptor
{
	u64 index_buffer_descriptor     = u64_invalid;
	u64 positions_buffer_descriptor = u64_invalid;
	u64 uvs_buffer_descriptor       = u64_invalid;
	u64 submesh_buffer_descriptor   = u64_invalid;
};

struct RenderMeshUpload
{
	Handle<vulkan::Buffer> dst_buffer;
	usize                  upload_offset;
	usize                  upload_size;
};

Renderer Renderer::create(u64 window_handle, AssetManager *asset_manager)
{
	Renderer renderer;
	renderer.base          = SimpleRenderer::create(window_handle);
	renderer.asset_manager = asset_manager;
	renderer.mesh_renderer.mesh_uuid_map = exo::IndexMap::with_capacity(64);
	return renderer;
}

void Renderer::draw(const RenderWorld &world)
{
	auto &device = base.device;

	// Upload meshes
	for (const auto &instance : world.drawable_instances) {
		auto mesh_uuid  = instance.mesh_asset;
		auto mesh_asset = static_cast<Mesh *>(this->asset_manager->get_asset(mesh_uuid).value());
		if (this->mesh_renderer.mesh_uuid_map.at(hash_value(mesh_uuid)) == None) {
			RenderMesh render_mesh       = {};
			render_mesh.mesh_asset       = mesh_uuid;
			render_mesh.index_buffer     = device.create_buffer({
					.name         = "Index buffer",
					.size         = mesh_asset->indices.size() * sizeof(u32),
					.usage        = vulkan::index_buffer_usage,
					.memory_usage = vulkan::MemoryUsage::GPU_ONLY,
            });
			render_mesh.positions_buffer = device.create_buffer({
				.name         = "Positions buffer",
				.size         = mesh_asset->positions.size() * sizeof(float4),
				.usage        = vulkan::storage_buffer_usage,
				.memory_usage = vulkan::MemoryUsage::GPU_ONLY,
			});
			render_mesh.uvs_buffer       = device.create_buffer({
					  .name         = "UV buffer",
					  .size         = mesh_asset->uvs.size() * sizeof(float2),
					  .usage        = vulkan::storage_buffer_usage,
					  .memory_usage = vulkan::MemoryUsage::GPU_ONLY,
            });
			render_mesh.submesh_buffer   = device.create_buffer({
				  .name         = "Submesh buffer",
				  .size         = mesh_asset->submeshes.size() * sizeof(SubmeshDescriptor),
				  .usage        = vulkan::storage_buffer_usage,
				  .memory_usage = vulkan::MemoryUsage::GPU_ONLY,
            });
			auto render_mesh_handle      = this->mesh_renderer.render_meshes.add(std::move(render_mesh));
			this->mesh_renderer.mesh_uuid_map.insert(hash_value(mesh_uuid), to_u64(render_mesh_handle));
		}
	}

	Vec<RenderMeshUpload> render_mesh_uploads;
	for (auto [handle, p_render_mesh] : this->mesh_renderer.render_meshes) {
		if (!p_render_mesh->is_uploaded) {
			auto *mesh_asset   = static_cast<Mesh *>(this->asset_manager->get_asset(p_render_mesh->mesh_asset).value());
			auto indices_size = device.get_buffer_size(p_render_mesh->index_buffer);
			auto positions_size = device.get_buffer_size(p_render_mesh->positions_buffer);
			auto uvs_size       = device.get_buffer_size(p_render_mesh->uvs_buffer);
			auto submeshes_size = device.get_buffer_size(p_render_mesh->submesh_buffer);
			auto total_size     = indices_size + positions_size + uvs_size + submeshes_size;

			auto [p_upload_data, upload_offset] = base.upload_buffer.allocate(total_size);
			if (p_upload_data) {
				fmt::print("[Renderer] Uploading mesh asset {} with {} vertices\n", mesh_asset->uuid, mesh_asset->positions.size());

				auto p_indices   = mesh_asset->indices.data();
				auto p_positions = mesh_asset->positions.data();
				auto p_uvs       = mesh_asset->uvs.data();

				auto *cursor = p_upload_data;
				std::memcpy(cursor, p_indices, indices_size);
				cursor = exo::ptr_offset(cursor, indices_size);
				render_mesh_uploads.push_back(RenderMeshUpload{
					.dst_buffer    = p_render_mesh->index_buffer,
					.upload_offset = upload_offset,
					.upload_size   = indices_size,
				});

				std::memcpy(cursor, p_positions, positions_size);
				cursor = exo::ptr_offset(cursor, positions_size);
				render_mesh_uploads.push_back(RenderMeshUpload{
					.dst_buffer    = p_render_mesh->positions_buffer,
					.upload_offset = upload_offset + indices_size,
					.upload_size   = positions_size,
				});

				std::memcpy(cursor, p_uvs, uvs_size);
				cursor = exo::ptr_offset(cursor, uvs_size);
				render_mesh_uploads.push_back(RenderMeshUpload{
					.dst_buffer    = p_render_mesh->uvs_buffer,
					.upload_offset = upload_offset + indices_size + positions_size,
					.upload_size   = uvs_size,
				});

				for (usize i_submesh = 0; i_submesh < mesh_asset->submeshes.size(); ++i_submesh) {
					auto *p_upload_submeshes                   = reinterpret_cast<SubmeshDescriptor *>(p_upload_data);
					p_upload_submeshes[i_submesh].first_index  = mesh_asset->submeshes[i_submesh].first_index;
					p_upload_submeshes[i_submesh].first_vertex = mesh_asset->submeshes[i_submesh].first_vertex;
					p_upload_submeshes[i_submesh].index_count  = mesh_asset->submeshes[i_submesh].index_count;
					p_upload_submeshes[i_submesh].i_material   = u32_invalid;
				}
				render_mesh_uploads.push_back(RenderMeshUpload{
					.dst_buffer    = p_render_mesh->submesh_buffer,
					.upload_offset = upload_offset + indices_size + positions_size + uvs_size,
					.upload_size   = submeshes_size,
				});

				p_render_mesh->is_uploaded = true;
			}
			else {
				fmt::print("[Renderer] Not enough space in upload buffer for mesh asset {}\n", mesh_asset->uuid);
			}
		}
	}

	if (!render_mesh_uploads.empty()) {
		auto uploads_span = std::span{render_mesh_uploads};
		base.render_graph.raw_pass([uploads_span](RenderGraph & /*graph*/, PassApi &api, vulkan::ComputeWork &cmd) {
			for (const auto &upload : uploads_span) {
				std::tuple<usize, usize, usize> offsets_size = {upload.upload_offset, 0, upload.upload_size};
				cmd.copy_buffer(api.upload_buffer.buffer, upload.dst_buffer, std::span{&offsets_size, 1});
			}
			});
	}

	auto intermediate_buffer = base.render_graph.output(TextureDesc{
		.name = "render buffer desc",
		.size = TextureSize::screen_relative(float2(1.0, 1.0)),
	});
	base.render_graph.graphic_pass(intermediate_buffer,
		[](RenderGraph & /*graph*/, PassApi & /*api*/, vulkan::GraphicsWork & /*cmd*/) {});
	base.render(intermediate_buffer, 1.0);
}
