#include "mesh_renderer.h"
#include "assets/asset_id.h"
#include "assets/asset_manager.h"
#include "assets/material.h"
#include "assets/mesh.h"
#include "assets/texture.h"
#include "engine/camera.h"
#include "engine/render_world.h"
#include "exo/collections/span.h"
#include "exo/macros/packed.h"
#include "render/bindings.h"
#include "render/shader_watcher.h"
#include "render/simple_renderer.h" // for FRAME_QUEUE_LENGTH...
#include "render/vulkan/device.h"
#include "render/vulkan/image.h"
#include <bit>

struct SubmeshDescriptor
{
	u32 i_material   = u32_invalid;
	u32 first_index  = u32_invalid;
	u32 first_vertex = u32_invalid;
	u32 index_count  = u32_invalid;
};
static_assert(sizeof(SubmeshDescriptor) == sizeof(float4));

struct MeshDescriptor
{
	u32 index_buffer_descriptor     = u32_invalid;
	u32 positions_buffer_descriptor = u32_invalid;
	u32 uvs_buffer_descriptor       = u32_invalid;
	u32 submesh_buffer_descriptor   = u32_invalid;
};
static_assert(sizeof(MeshDescriptor) == sizeof(float4));

PACKED(struct InstanceDescriptor {
	float4x4 transform;
	u32      i_mesh_descriptor;
	u32      padding0;
	u32      padding1;
	u32      padding2;
})
static_assert(sizeof(InstanceDescriptor) == 5 * sizeof(float4));

PACKED(struct MaterialDescriptor {
	float4 base_color_factor          = float4(1.0f);
	float4 emissive_factor            = float4(0.0f);
	float  metallic_factor            = 0.0f;
	float  roughness_factor           = 0.0f;
	u32    base_color_texture         = u32_invalid;
	u32    normal_texture             = u32_invalid;
	u32    metallic_roughness_texture = u32_invalid;
	float  rotation                   = 0.0f;
	float2 offset                     = float2(0.0f);
	float2 scale                      = float2(1.0f);
	float2 pad00;
})
static_assert(sizeof(MaterialDescriptor) == 5 * sizeof(float4));

MeshRenderer MeshRenderer::create(vulkan::Device &device)
{
	MeshRenderer renderer      = {};
	renderer.mesh_uuid_map     = exo::Map<AssetId, Handle<RenderMesh>>::with_capacity(64);
	renderer.material_uuid_map = exo::Map<AssetId, Handle<RenderMaterial>>::with_capacity(64);
	renderer.texture_uuid_map  = exo::Map<AssetId, Handle<RenderTexture>>::with_capacity(64);
	renderer.instances_buffer  = RingBuffer::create(device,
		 {
			 .name               = "Instances buffer",
			 .size               = 128_KiB,
			 .gpu_usage          = vulkan::storage_buffer_usage,
			 .frame_queue_length = FRAME_QUEUE_LENGTH,
        });
	renderer.meshes_buffer     = device.create_buffer({
			.name  = "Meshes buffer",
			.size  = sizeof(MeshDescriptor) * (1 << 15),
			.usage = vulkan::storage_buffer_usage,
    });
	renderer.materials_buffer  = device.create_buffer({
		 .name  = "Materials buffer",
		 .size  = sizeof(MaterialDescriptor) * (1 << 20),
		 .usage = vulkan::storage_buffer_usage,
    });

	vulkan::GraphicsState gui_state = {};
	gui_state.vertex_shader         = device.create_shader(SHADER_PATH("simple_mesh.vert.glsl.spv"));
	gui_state.fragment_shader       = device.create_shader(SHADER_PATH("simple_mesh.frag.glsl.spv"));
	gui_state.attachments_format    = {
		   .attachments_format = {VK_FORMAT_R8G8B8A8_UNORM},
		   .depth_format       = Some(VK_FORMAT_D32_SFLOAT),
    };

	renderer.simple_program          = device.create_program("simple mesh renderer", gui_state);
	vulkan::RenderState render_state = {};
	render_state.depth.test          = Some(VK_COMPARE_OP_GREATER_OR_EQUAL);
	render_state.depth.enable_write  = true;
	device.compile_graphics_state(renderer.simple_program, render_state);

	return renderer;
}

static Handle<RenderTexture> get_or_create_texture(
	MeshRenderer &renderer, AssetManager *asset_manager, vulkan::Device &device, const AssetId &texture_uuid)
{
	auto texture = asset_manager->load_asset_t<Texture>(texture_uuid);

	auto *render_texture_handle = renderer.texture_uuid_map.at(texture_uuid);
	if (render_texture_handle) {
		return *render_texture_handle;
	}

	RenderTexture render_texture = {};
	render_texture.texture_asset = texture_uuid;

	VkFormat vk_format = VK_FORMAT_R8G8B8A8_UNORM;
	switch (texture->format) {
	case PixelFormat::R8G8B8A8_UNORM: {
		vk_format = VK_FORMAT_R8G8B8A8_UNORM;
		break;
	}
	default: {
		ASSERT(false);
	}
	}

	ASSERT(texture->mip_offsets.len() == 1);
	ASSERT(texture->levels == 1);
	ASSERT(texture->depth == 1);

	render_texture.image = device.create_image({
		.name = texture->name,
		.size =
			{
				texture->width,
				texture->height,
				texture->depth,
			},
		.format = vk_format,
	});

	// Add the texture to the map
	auto handle = renderer.render_textures.add(std::move(render_texture));
	renderer.texture_uuid_map.insert(texture_uuid, handle);
	return handle;
}

static Handle<RenderMaterial> get_or_create_material(
	MeshRenderer &renderer, AssetManager *asset_manager, vulkan::Device &device, const AssetId &material_uuid)
{
	auto material = asset_manager->load_asset_t<Material>(material_uuid);

	auto *render_material_handle = renderer.material_uuid_map.at(material_uuid);
	if (render_material_handle) {
		return *render_material_handle;
	}

	RenderMaterial render_material = {};
	render_material.material_asset = material_uuid;
	if (material->base_color_texture.is_valid()) {
		render_material.base_color_texture =
			get_or_create_texture(renderer, asset_manager, device, material->base_color_texture);
	}
	if (material->normal_texture.is_valid()) {
		render_material.normal_texture =
			get_or_create_texture(renderer, asset_manager, device, material->normal_texture);
	}
	if (material->metallic_roughness_texture.is_valid()) {
		render_material.metallic_roughness_texture =
			get_or_create_texture(renderer, asset_manager, device, material->metallic_roughness_texture);
	}

	// Add the material to the map
	auto handle = renderer.render_materials.add(std::move(render_material));
	renderer.material_uuid_map.insert(material_uuid, handle);
	return handle;
}

static Handle<RenderMesh> get_or_create_mesh(
	MeshRenderer &renderer, AssetManager *asset_manager, vulkan::Device &device, const AssetId &mesh_uuid)
{
	ASSERT(asset_manager->is_loaded(mesh_uuid));
	auto mesh = asset_manager->load_asset_t<Mesh>(mesh_uuid);

	auto *render_mesh_handle = renderer.mesh_uuid_map.at(mesh_uuid);
	if (render_mesh_handle) {
		return *render_mesh_handle;
	}

	RenderMesh render_mesh       = {};
	render_mesh.index_buffer     = device.create_buffer({
			.name  = "Index buffer",
			.size  = mesh->indices_byte_size,
			.usage = vulkan::index_buffer_usage | vulkan::storage_buffer_usage,
    });
	render_mesh.positions_buffer = device.create_buffer({
		.name  = "Positions buffer",
		.size  = mesh->positions_byte_size,
		.usage = vulkan::storage_buffer_usage,
	});
	render_mesh.uvs_buffer       = device.create_buffer({
			  .name  = "UV buffer",
			  .size  = mesh->uvs_byte_size,
			  .usage = vulkan::storage_buffer_usage,
    });
	render_mesh.submesh_buffer   = device.create_buffer({
		  .name  = "Submesh buffer",
		  .size  = mesh->submeshes.len() * sizeof(SubmeshDescriptor),
		  .usage = vulkan::storage_buffer_usage,
    });
	render_mesh.mesh_asset       = mesh_uuid;

	for (const auto &submesh : mesh->submeshes) {
		auto render_material_handle = get_or_create_material(renderer, asset_manager, device, submesh.material);

		render_mesh.render_submeshes.push(RenderSubmesh{
			.material    = render_material_handle,
			.index_count = submesh.index_count,
			.first_index = submesh.first_index,
		});
	}

	auto handle = renderer.render_meshes.add(std::move(render_mesh));
	renderer.mesh_uuid_map.insert(mesh_uuid, handle);
	return handle;
}

void register_upload_nodes(RenderGraph &graph,
	MeshRenderer                       &mesh_renderer,
	vulkan::Device                     &device,
	RingBuffer                         &upload_buffer,
	AssetManager                       *asset_manager,
	const RenderWorld                  &world)
{
	mesh_renderer.instances_buffer.start_frame();
	mesh_renderer.drawcalls.clear();

	// Gather instances of uploaded meshes
	for (const auto &instance : world.drawable_instances) {
		auto        render_mesh_handle = get_or_create_mesh(mesh_renderer, asset_manager, device, instance.mesh_asset);
		const auto &render_mesh        = mesh_renderer.render_meshes.get(render_mesh_handle);
		if (!render_mesh.is_uploaded) {
			continue;
		}

		auto [p_data, instance_bytes_offset] =
			mesh_renderer.instances_buffer.allocate(sizeof(InstanceDescriptor), sizeof(InstanceDescriptor));
		ASSERT(!p_data.empty());
		auto p_instance                 = exo::reinterpret_span<InstanceDescriptor>(p_data);
		p_instance[0].i_mesh_descriptor = render_mesh_handle.get_index();
		p_instance[0].transform         = instance.world_transform;

		ASSERT(instance_bytes_offset % sizeof(InstanceDescriptor) == 0);

		for (u32 i_submesh = 0; i_submesh < render_mesh.render_submeshes.len(); ++i_submesh) {
			const auto &submesh = render_mesh.render_submeshes[i_submesh];
			mesh_renderer.drawcalls.push(SimpleDraw{
				.instance_offset = static_cast<u32>(instance_bytes_offset / sizeof(InstanceDescriptor)),
				.instance_count  = 1,
				.index_count     = submesh.index_count,
				.index_offset    = submesh.first_index,
				.index_buffer    = render_mesh.index_buffer,
				.i_submesh       = i_submesh,
			});
		}
	}

	// Upload new textures
	for (auto [handle, p_render_texture] : mesh_renderer.render_textures) {
		if (p_render_texture->frame_uploaded == u64_invalid) {

			auto *texture                       = asset_manager->load_asset_t<Texture>(p_render_texture->texture_asset);
			auto [p_upload_data, upload_offset] = upload_buffer.allocate(texture->pixels_data_size);

			if (p_upload_data.empty()) {
				continue;
			}

			printf("[Renderer] Uploading texture asset %s at offset 0x%zx frame #%u\n",
				texture->uuid.name.c_str(),
				upload_offset,
				upload_buffer.i_frame);

			asset_manager->read_blob(texture->pixels_hash, p_upload_data);
			mesh_renderer.image_uploads.push(RenderImageUpload{
				.dst_image     = p_render_texture->image,
				.upload_offset = upload_offset,
				.upload_size   = texture->pixels_data_size,
				.extent        = int3(texture->width, texture->height, texture->depth),
			});
			p_render_texture->frame_uploaded = graph.i_frame + 3;
		}
	}

	// Upload new materials
	for (auto [handle, p_render_material] : mesh_renderer.render_materials) {
		auto is_texture_uploaded = [&](MeshRenderer &mesh_renderer, Handle<RenderTexture> texture_handle) -> bool {
			return !texture_handle.is_valid() ||
			       (mesh_renderer.render_textures.get(texture_handle).frame_uploaded <= graph.i_frame);
		};

		const bool textures_uploaded =
			is_texture_uploaded(mesh_renderer, p_render_material->base_color_texture) &&
			is_texture_uploaded(mesh_renderer, p_render_material->normal_texture) &&
			is_texture_uploaded(mesh_renderer, p_render_material->metallic_roughness_texture);

		if (!p_render_material->is_uploaded && textures_uploaded) {
			auto [p_upload_data, upload_offset] = upload_buffer.allocate(sizeof(MaterialDescriptor));
			if (p_upload_data.empty()) {
				continue;
			}

			auto *material_asset    = asset_manager->load_asset_t<Material>(p_render_material->material_asset);
			auto  p_upload_material = exo::reinterpret_span<MaterialDescriptor>(p_upload_data);

			printf("[Renderer] Uploading material asset %s at offset 0x%zx frame #%u\n",
				material_asset->uuid.name.c_str(),
				upload_offset,
				upload_buffer.i_frame);

			p_upload_material[0]                   = MaterialDescriptor{};
			p_upload_material[0].base_color_factor = material_asset->base_color_factor;
			p_upload_material[0].emissive_factor   = material_asset->emissive_factor;
			p_upload_material[0].metallic_factor   = material_asset->metallic_factor;
			p_upload_material[0].roughness_factor  = material_asset->roughness_factor;
			p_upload_material[0].rotation          = material_asset->uv_transform.rotation;
			p_upload_material[0].offset            = material_asset->uv_transform.offset;
			p_upload_material[0].scale             = material_asset->uv_transform.scale;

			if (p_render_material->base_color_texture.is_valid()) {
				auto image = mesh_renderer.render_textures.get(p_render_material->base_color_texture).image;
				p_upload_material[0].base_color_texture = device.get_image_sampled_index(image);
			}
			if (p_render_material->normal_texture.is_valid()) {
				auto image = mesh_renderer.render_textures.get(p_render_material->normal_texture).image;
				p_upload_material[0].normal_texture = device.get_image_sampled_index(image);
			}
			if (p_render_material->metallic_roughness_texture.is_valid()) {
				auto image = mesh_renderer.render_textures.get(p_render_material->metallic_roughness_texture).image;
				p_upload_material[0].metallic_roughness_texture = device.get_image_sampled_index(image);
			}

			mesh_renderer.buffer_uploads.push(RenderUploads{
				.dst_buffer    = mesh_renderer.materials_buffer,
				.dst_offset    = handle.get_index() * sizeof(MaterialDescriptor),
				.upload_offset = upload_offset,
				.upload_size   = sizeof(MaterialDescriptor),
			});

			p_render_material->is_uploaded = true;
			break;
		}
	}

	// Upload new meshes
	for (auto [handle, p_render_mesh] : mesh_renderer.render_meshes) {
		bool materials_uploaded = true;
		for (u32 i_submesh = 0; i_submesh < p_render_mesh->render_submeshes.len() && materials_uploaded; ++i_submesh) {
			const auto &render_submesh = p_render_mesh->render_submeshes[i_submesh];
			if (render_submesh.material.is_valid()) {
				materials_uploaded = mesh_renderer.render_materials.get(render_submesh.material).is_uploaded;
			}
		}

		if (!p_render_mesh->is_uploaded && materials_uploaded) {

			auto indices_size         = device.get_buffer_size(p_render_mesh->index_buffer);
			auto positions_size       = device.get_buffer_size(p_render_mesh->positions_buffer);
			auto uvs_size             = device.get_buffer_size(p_render_mesh->uvs_buffer);
			auto submeshes_size       = device.get_buffer_size(p_render_mesh->submesh_buffer);
			auto mesh_descriptor_size = sizeof(MeshDescriptor);
			auto total_size = indices_size + positions_size + uvs_size + submeshes_size + mesh_descriptor_size;

			auto [p_upload_data, upload_offset] = upload_buffer.allocate(total_size);
			if (p_upload_data.empty()) {
				continue;
			}

			auto *mesh_asset = asset_manager->load_asset_t<Mesh>(p_render_mesh->mesh_asset);
			printf("[Renderer] Uploading mesh asset %s at offset 0x%zx frame #%u\n",
				mesh_asset->uuid.name.c_str(),
				upload_offset,
				upload_buffer.i_frame);

			auto bread = asset_manager->read_blob(mesh_asset->indices_hash, p_upload_data);
			mesh_renderer.buffer_uploads.push(RenderUploads{
				.dst_buffer    = p_render_mesh->index_buffer,
				.upload_offset = upload_offset,
				.upload_size   = indices_size,
			});

			bread += asset_manager->read_blob(mesh_asset->positions_hash, p_upload_data.subspan(bread));
			mesh_renderer.buffer_uploads.push(RenderUploads{
				.dst_buffer    = p_render_mesh->positions_buffer,
				.upload_offset = upload_offset + indices_size,
				.upload_size   = positions_size,
			});

			bread += asset_manager->read_blob(mesh_asset->uvs_hash, p_upload_data.subspan(bread));
			mesh_renderer.buffer_uploads.push(RenderUploads{
				.dst_buffer    = p_render_mesh->uvs_buffer,
				.upload_offset = upload_offset + indices_size + positions_size,
				.upload_size   = uvs_size,
			});

			auto p_upload_submeshes = exo::reinterpret_span<SubmeshDescriptor>(p_upload_data.subspan(bread));
			for (usize i_submesh = 0; i_submesh < mesh_asset->submeshes.len(); ++i_submesh) {
				const auto &render_submesh = p_render_mesh->render_submeshes[i_submesh];

				p_upload_submeshes[i_submesh].first_index  = mesh_asset->submeshes[i_submesh].first_index;
				p_upload_submeshes[i_submesh].first_vertex = mesh_asset->submeshes[i_submesh].first_vertex;
				p_upload_submeshes[i_submesh].index_count  = mesh_asset->submeshes[i_submesh].index_count;
				if (render_submesh.material.is_valid() &&
					mesh_renderer.render_materials.get(render_submesh.material).is_uploaded) {
					p_upload_submeshes[i_submesh].i_material = render_submesh.material.get_index();
				} else {
					p_upload_submeshes[i_submesh].i_material = u32_invalid;
				}
			}
			bread += submeshes_size;
			mesh_renderer.buffer_uploads.push(RenderUploads{
				.dst_buffer    = p_render_mesh->submesh_buffer,
				.upload_offset = upload_offset + indices_size + positions_size + uvs_size,
				.upload_size   = submeshes_size,
			});

			auto p_upload_descriptor = exo::reinterpret_span<MeshDescriptor>(p_upload_data.subspan(bread));
			p_upload_descriptor[0].index_buffer_descriptor =
				device.get_buffer_storage_index(p_render_mesh->index_buffer);
			p_upload_descriptor[0].positions_buffer_descriptor =
				device.get_buffer_storage_index(p_render_mesh->positions_buffer);
			p_upload_descriptor[0].uvs_buffer_descriptor = device.get_buffer_storage_index(p_render_mesh->uvs_buffer);
			p_upload_descriptor[0].submesh_buffer_descriptor =
				device.get_buffer_storage_index(p_render_mesh->submesh_buffer);
			mesh_renderer.buffer_uploads.push(RenderUploads{
				.dst_buffer    = mesh_renderer.meshes_buffer,
				.dst_offset    = handle.get_index() * sizeof(MeshDescriptor),
				.upload_offset = upload_offset + indices_size + positions_size + uvs_size + submeshes_size,
				.upload_size   = sizeof(MeshDescriptor),
			});

			p_render_mesh->is_uploaded = true;
			break;
		}
	}

	// Submit upload commands
	if (!mesh_renderer.image_uploads.is_empty()) {
		exo::Span<RenderImageUpload> uploads_span = mesh_renderer.image_uploads;
		graph.raw_pass([uploads_span](RenderGraph & /*graph*/, PassApi &api, vulkan::ComputeWork &cmd) {
			for (const auto &upload : uploads_span) {
				auto copy = VkBufferImageCopy{
					.bufferOffset = upload.upload_offset,
					.imageSubresource =
						{
							.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
							.layerCount = 1,
						},
					.imageOffset =
						{
							.x = 0,
							.y = 0,
							.z = 0,
						},
					.imageExtent =
						{
							.width  = u32(upload.extent.x),
							.height = u32(upload.extent.y),
							.depth  = u32(upload.extent.z),
						},
				};

				cmd.barrier(upload.dst_image, vulkan::ImageUsage::TransferDst);
				cmd.copy_buffer_to_image(api.upload_buffer.buffer, upload.dst_image, exo::Span{&copy, 1});
				cmd.barrier(upload.dst_image, vulkan::ImageUsage::GraphicsShaderRead);
			}
		});
		mesh_renderer.image_uploads.clear();
	}
	if (!mesh_renderer.buffer_uploads.is_empty()) {
		exo::Span<RenderUploads> uploads_span = mesh_renderer.buffer_uploads;
		graph.raw_pass([uploads_span](RenderGraph & /*graph*/, PassApi &api, vulkan::ComputeWork &cmd) {
			for (const auto &upload : uploads_span) {
				std::tuple<usize, usize, usize> offsets_size = {upload.upload_offset,
					upload.dst_offset,
					upload.upload_size};
				cmd.copy_buffer(api.upload_buffer.buffer, upload.dst_buffer, exo::Span{&offsets_size, 1});
			}
		});
		mesh_renderer.buffer_uploads.clear();
	}

	mesh_renderer.view                 = world.main_camera_view;
	mesh_renderer.projection           = world.main_camera_projection;
	mesh_renderer.instances_descriptor = device.get_buffer_storage_index(mesh_renderer.instances_buffer.buffer);
	mesh_renderer.meshes_descriptor    = device.get_buffer_storage_index(mesh_renderer.meshes_buffer);
	mesh_renderer.materials_descriptor = device.get_buffer_storage_index(mesh_renderer.materials_buffer);
}

void register_graphics_nodes(RenderGraph &graph, MeshRenderer &mesh_renderer, Handle<TextureDesc> output)
{
	exo::Span<SimpleDraw> drawcalls_span       = mesh_renderer.drawcalls;
	auto                  instances_descriptor = mesh_renderer.instances_descriptor;
	auto                  meshes_descriptor    = mesh_renderer.meshes_descriptor;
	auto                  materials_descriptor = mesh_renderer.materials_descriptor;
	auto                  simple_program       = mesh_renderer.simple_program;
	auto                  view                 = mesh_renderer.view;
	auto                  output_size          = graph.image_size(output);
	auto                  projection           = mesh_renderer.projection;

	auto depth_buffer = graph.output(TextureDesc{
		.name   = "depth buffer desc",
		.size   = TextureSize::absolute(output_size.xy()),
		.format = VK_FORMAT_D32_SFLOAT,
	});

	graph.graphic_pass(output,
		depth_buffer,
		[drawcalls_span,
			instances_descriptor,
			meshes_descriptor,
			materials_descriptor,
			simple_program,
			view,
			projection](RenderGraph & /*graph*/, PassApi &api, vulkan::GraphicsWork &cmd) {
			auto last_index_buffer = Handle<vulkan::Buffer>::invalid();
			for (const auto &drawcall : drawcalls_span) {
				PACKED(struct Options {
					float4x4 view;
					float4x4 projection;
					u32      instances_descriptor;
					u32      meshes_descriptor;
					u32      i_submesh;
					u32      materials_descriptor;
				})

				auto options          = bindings::bind_option_struct<Options>(api.device, api.uniform_buffer, cmd);
				options[0].view       = view;
				options[0].projection = projection;
				options[0].instances_descriptor = instances_descriptor;
				options[0].meshes_descriptor    = meshes_descriptor;
				options[0].i_submesh            = drawcall.i_submesh;
				options[0].materials_descriptor = materials_descriptor;

				cmd.bind_pipeline(simple_program, 0);

				if (drawcall.index_buffer != last_index_buffer) {
					cmd.bind_index_buffer(drawcall.index_buffer, VK_INDEX_TYPE_UINT32, 0);
					last_index_buffer = drawcall.index_buffer;
				}

				cmd.draw_indexed(vulkan::DrawIndexedOptions{
					.vertex_count    = drawcall.index_count,
					.instance_count  = drawcall.instance_count,
					.index_offset    = drawcall.index_offset,
					.vertex_offset   = 0,
					.instance_offset = drawcall.instance_offset,
				});
			}
		});
}
