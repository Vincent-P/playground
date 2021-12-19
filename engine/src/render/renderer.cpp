#include "render/renderer.h"

#include <exo/logger.h>
#include <exo/maths/matrices.h>
#include <exo/maths/numerics.h>
#include <exo/maths/quaternion.h>
#include <exo/memory/scope_stack.h>
#include <exo/os/mapped_file.h>

#include "assets/asset_manager.h"
#include "assets/material.h"
#include "assets/mesh.h"
#include "assets/texture.h"

#include "render/base_renderer.h"
#include "render/bvh.h"
#include "render/unified_buffer_storage.h"
#include "render/render_world.h"

#include "gameplay/entity.h"
#include "gameplay/components/camera_component.h"


#include "ui.h"
#include "camera.h"

#include <imgui.h>

namespace
{
uint3 dispatch_size(int3 size, i32 threads)
{
    return uint3(int3{
        (size.x / threads) + i32(size.x % threads != 0),
        (size.y / threads) + i32(size.y % threads != 0),
        (size.z / threads) + i32(size.z % threads != 0),
    });
}

VkFormat to_vk(PixelFormat pformat)
{
    // clang-format off
    switch (pformat)
    {
    case PixelFormat::R8G8B8A8_UNORM: return VK_FORMAT_R8G8B8A8_UNORM;
    case PixelFormat::R8G8B8A8_SRGB: return VK_FORMAT_R8G8B8A8_SRGB;
    case PixelFormat::BC7_SRGB: return VK_FORMAT_BC7_SRGB_BLOCK;
    case PixelFormat::BC7_UNORM: return VK_FORMAT_BC7_UNORM_BLOCK;
    case PixelFormat::BC4_UNORM: return VK_FORMAT_BC4_UNORM_BLOCK;
    case PixelFormat::BC5_UNORM: return VK_FORMAT_BC5_UNORM_BLOCK;
    default: ASSERT(false);
    }
    // clang-format on
    exo::unreachable();
}

void recreate_framebuffers(Renderer &r)
{
    auto &device   = r.base_renderer->device;
    auto &surface  = r.base_renderer->surface;
    auto &settings = r.settings;

    device.wait_idle();

    settings.render_resolution = int2(surface.width, surface.height);

    int3 scaled_resolution = int3(int2(settings.resolution_scale * float2(settings.render_resolution)), 1);

    // Re-create images
    device.destroy_image(r.depth_buffer);
    device.destroy_image(r.hdr_buffer);
    device.destroy_image(r.ldr_buffer);
    device.destroy_image(r.visibility_buffer);
    device.destroy_image(r.history_buffers[0]);
    device.destroy_image(r.history_buffers[1]);

    r.visibility_buffer = device.create_image({
        .name   = "Visibility buffer",
        .size   = int3(settings.render_resolution, 1.0),
        .format = VK_FORMAT_R32G32_UINT,
        .usages = gfx::color_attachment_usage | gfx::storage_image_usage,
    });

    r.depth_buffer = device.create_image({
        .name   = "Depth buffer",
        .size   = scaled_resolution,
        .format = VK_FORMAT_D32_SFLOAT,
        .usages = gfx::depth_attachment_usage,
    });

    r.hdr_buffer = device.create_image({
        .name   = "HDR buffer",
        .size   = scaled_resolution,
        .format = VK_FORMAT_R16G16B16A16_SFLOAT,
        .usages = gfx::color_attachment_usage,
    });

    r.ldr_buffer = device.create_image({
        .name   = "LDR buffer",
        .size   = int3(settings.render_resolution, 1.0),
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .usages = gfx::color_attachment_usage,
    });

    for (u32 i_history = 0; i_history < 2; i_history += 1)
    {
        r.history_buffers[i_history] = device.create_image({
            .name   = fmt::format("History buffer #{}", i_history),
            .size   = int3(settings.render_resolution, 1.0),
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .usages = gfx::storage_image_usage,
        });
    }

    // Re-create framebuffers
    device.destroy_framebuffer(r.hdr_depth_fb);
    device.destroy_framebuffer(r.ldr_depth_fb);
    device.destroy_framebuffer(r.ldr_fb);

    r.visibility_depth_fb = device.create_framebuffer(
        {
            .width  = scaled_resolution.x,
            .height = scaled_resolution.y,
        },
        std::array{r.visibility_buffer},
        r.depth_buffer);

    r.hdr_depth_fb = device.create_framebuffer(
        {
            .width  = scaled_resolution.x,
            .height = scaled_resolution.y,
        },
        std::array{r.hdr_buffer},
        r.depth_buffer);

    r.ldr_depth_fb = device.create_framebuffer(
        {
            .width  = scaled_resolution.x,
            .height = scaled_resolution.y,
        },
        std::array{r.ldr_buffer},
        r.depth_buffer);

    r.ldr_fb = device.create_framebuffer(
        {
            .width  = settings.render_resolution.x,
            .height = settings.render_resolution.y,
        },
        std::array{r.ldr_buffer});
}

void upload_texture(Renderer &renderer, exo::UUID texture_uuid)
{
    auto &asset_manager    = *renderer.asset_manager;
    auto &streamer = renderer.base_renderer->streamer;
    auto &device           = renderer.base_renderer->device;
    auto &render_textures = renderer.render_textures;

    const auto *texture_asset = dynamic_cast<Texture *>(asset_manager.get_asset(texture_uuid).value());
    ASSERT(texture_asset);

    RenderTexture render_texture = {};
    render_texture.gpu = device.create_image({
        .name       = texture_asset->name,
        .size       = int3(texture_asset->width, texture_asset->height, texture_asset->depth),
        .mip_levels = static_cast<u32>(texture_asset->levels),
        .format     = to_vk(texture_asset->format),
    });

    streamer.upload(render_texture.gpu, *texture_asset);

    auto render_texture_handle = render_textures.add(std::move(render_texture));
    renderer.uploaded_textures[texture_uuid] = render_texture_handle;
}

bool is_texture_uploaded(Renderer &renderer, exo::UUID texture_uuid)
{
    return renderer.uploaded_textures.contains(texture_uuid);
}

void upload_material(Renderer &renderer, exo::UUID material_uuid)
{
    auto &asset_manager    = *renderer.asset_manager;
    auto &device           = renderer.base_renderer->device;
    auto &render_materials = renderer.render_materials;

    const auto *material_asset = dynamic_cast<Material *>(asset_manager.get_asset(material_uuid).value());
    ASSERT(material_asset);

    RenderMaterial render_material = {};

    RenderMaterialGPU render_material_gpu          = {};
    render_material_gpu.base_color_factor          = material_asset->base_color_factor;
    render_material_gpu.emissive_factor            = material_asset->emissive_factor;
    render_material_gpu.metallic_factor            = material_asset->metallic_factor;
    render_material_gpu.roughness_factor           = material_asset->roughness_factor;

    render_material_gpu.base_color_texture         = u32_invalid;
    render_material_gpu.normal_texture             = u32_invalid;
    render_material_gpu.metallic_roughness_texture = u32_invalid;

    render_material_gpu.rotation = material_asset->uv_transform.rotation;
    render_material_gpu.offset   = material_asset->uv_transform.offset;
    render_material_gpu.scale    = material_asset->uv_transform.scale;

    // Check textures
    if (material_asset->base_color_texture.is_valid())
    {
        if (!is_texture_uploaded(renderer, material_asset->base_color_texture))
        {
            upload_texture(renderer, material_asset->base_color_texture);
        }
        ASSERT(is_texture_uploaded(renderer, material_asset->base_color_texture));
        auto render_texture_handle = renderer.uploaded_textures.at(material_asset->base_color_texture);
        const auto &render_texture = *renderer.render_textures.get(render_texture_handle);
        render_material_gpu.base_color_texture = device.get_image_sampled_index(render_texture.gpu);
    }

    // find a free slot
    auto material_handle = render_materials.add(std::move(render_material));
    renderer.uploaded_materials[material_uuid] = material_handle;

    // upload to GPU
    ASSERT(material_handle.value() < device.get_buffer_size(renderer.materials_buffer) / sizeof(RenderMaterialGPU));
    auto *materials_gpu = reinterpret_cast<RenderMaterialGPU *>(device.map_buffer(renderer.materials_buffer));
    materials_gpu[material_handle.value()] = render_material_gpu;
}

bool is_material_uploaded(Renderer &renderer, exo::UUID material_uuid)
{
    return renderer.uploaded_materials.contains(material_uuid);
}

void upload_mesh(Renderer &renderer, exo::UUID mesh_uuid)
{
    auto &asset_manager = *renderer.asset_manager;
    auto &streamer = renderer.base_renderer->streamer;
    auto &device = renderer.base_renderer->device;
    auto &render_meshes = renderer.render_meshes;

    const auto *mesh_asset = dynamic_cast<Mesh*>(asset_manager.get_asset(mesh_uuid).value());
    ASSERT(mesh_asset);

    exo::logger::info("[Renderer] Uploading mesh asset {}\n", mesh_uuid);
    static BVHScratchMemory scratch_bvh = {};
    static BVH              bvh         = {};
    create_blas(scratch_bvh, bvh, mesh_asset->indices, mesh_asset->positions);

    RenderMesh render_mesh         = {};
    render_mesh.bvh_root           = bvh.nodes[0];
    render_mesh.submeshes.resize(mesh_asset->submeshes.size());

    for (u32 i_submesh = 0; i_submesh < render_mesh.submeshes.size(); i_submesh += 1)
    {
        auto material_uuid = mesh_asset->submeshes[i_submesh].material;
        if (!material_uuid.is_valid())
        {
            render_mesh.submeshes[i_submesh].i_material = 0;
            continue;
        }

        if (!is_material_uploaded(renderer, material_uuid))
        {
            upload_material(renderer, material_uuid);
        }

        render_mesh.submeshes[i_submesh].i_material = renderer.uploaded_materials[material_uuid].value();
    }

    for (u32 i_submesh = 0; i_submesh < render_mesh.submeshes.size(); i_submesh += 1)
    {
        render_mesh.submeshes[i_submesh].first_index  = mesh_asset->submeshes[i_submesh].first_index;
        render_mesh.submeshes[i_submesh].first_vertex = mesh_asset->submeshes[i_submesh].first_vertex;
        render_mesh.submeshes[i_submesh].index_count  = mesh_asset->submeshes[i_submesh].index_count;
    }


    RenderMeshGPU render_mesh_gpu;
    render_mesh_gpu.first_position = renderer.vertex_positions_buffer.allocate(mesh_asset->positions.size());
    render_mesh_gpu.first_index    = renderer.index_buffer.allocate(mesh_asset->indices.size());
    render_mesh_gpu.bvh_root       = renderer.bvh_nodes_buffer.allocate(bvh.nodes.size());
    render_mesh_gpu.first_submesh  = renderer.submeshes_buffer.allocate(mesh_asset->submeshes.size());
    render_mesh_gpu.first_uv       = renderer.vertex_uvs_buffer.allocate(mesh_asset->uvs.size());

    streamer.upload(renderer.vertex_positions_buffer.buffer,
                    mesh_asset->positions.data(),
                    mesh_asset->positions.size() * sizeof(float4),
                    render_mesh_gpu.first_position * sizeof(float4));
    streamer.upload(renderer.vertex_uvs_buffer.buffer,
                    mesh_asset->uvs.data(),
                    mesh_asset->uvs.size() * sizeof(float2),
                    render_mesh_gpu.first_uv * sizeof(float2));
    streamer.upload(renderer.index_buffer.buffer,
                    mesh_asset->indices.data(),
                    mesh_asset->indices.size() * sizeof(u32),
                    render_mesh_gpu.first_index * sizeof(u32));
    streamer.upload(renderer.bvh_nodes_buffer.buffer,
                    bvh.nodes.data(),
                    bvh.nodes.size() * sizeof(BVHNode),
                    render_mesh_gpu.bvh_root * sizeof(BVHNode));
    streamer.upload(renderer.submeshes_buffer.buffer,
                    render_mesh.submeshes.data(),
                    render_mesh.submeshes.size() * sizeof(RenderSubMesh),
                    render_mesh_gpu.first_submesh * sizeof(RenderSubMesh));


    auto mesh_handle = render_meshes.add(std::move(render_mesh));

    ASSERT(mesh_handle.value() < device.get_buffer_size(renderer.meshes_buffer) / sizeof(RenderMeshGPU));
    auto *meshes_gpu = reinterpret_cast<RenderMeshGPU *>(device.map_buffer(renderer.meshes_buffer));
    meshes_gpu[mesh_handle.value()] = render_mesh_gpu;

    renderer.uploaded_meshes[mesh_asset->uuid] = mesh_handle;
}

bool is_mesh_uploaded(Renderer &renderer, exo::UUID mesh_uuid)
{
    return renderer.uploaded_meshes.contains(mesh_uuid);
}

} // namespace

Renderer *Renderer::create(exo::ScopeStack &scope, exo::Window *window, AssetManager *_asset_manager)
{
    auto *renderer          = scope.allocate<Renderer>();
    renderer->asset_manager = _asset_manager;
    renderer->base_renderer = BaseRenderer::create(scope,
                                                   window,
                                                   {
                                                       .push_constant_layout  = {.size = sizeof(PushConstants)},
                                                       .buffer_device_address = false,
                                                   });

    auto &device  = renderer->base_renderer->device;
    auto &surface = renderer->base_renderer->surface;

    renderer->instances_data = RingBuffer::create(device,
                                                 {
                                                     .name      = "Instances data",
                                                     .size      = 64_MiB,
                                                     .gpu_usage = gfx::storage_buffer_usage,
                                                 },
                                                 false);

    renderer->predicate_buffer = device.create_buffer({
        .name  = "Instance visibility",
        .size  = 1_MiB,
        .usage = gfx::storage_buffer_usage,
    });

    renderer->group_sum_reduction = device.create_buffer({
        .name  = "Group sum reduction",
        .size  = 1_MiB,
        .usage = gfx::storage_buffer_usage,
    });

    renderer->scanned_indices = device.create_buffer({
        .name  = "Culled instances scan indices",
        .size  = 1_MiB,
        .usage = gfx::storage_buffer_usage,
    });

    renderer->culled_instances_compact_indices = device.create_buffer({
        .name  = "Culled instances compact indices",
        .size  = 1_MiB,
        .usage = gfx::storage_buffer_usage,
    });

    renderer->submesh_instances_data = RingBuffer::create(device,
                                                         {
                                                             .name      = "Submesh Instances data",
                                                             .size      = 8_MiB,
                                                             .gpu_usage = gfx::storage_buffer_usage,
                                                         },
                                                         false);

    renderer->meshes_buffer = device.create_buffer({
        .name         = "Meshes description buffer",
        .size         = 2_MiB,
        .usage        = gfx::storage_buffer_usage,
        .memory_usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
    });

    renderer->tlas_buffer = device.create_buffer({
        .name         = "TLAS BVH buffer",
        .size         = 32_MiB,
        .usage        = gfx::storage_buffer_usage,
        .memory_usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
    });

    renderer->draw_arguments = device.create_buffer({
        .name  = "Indirect Draw arguments",
        .size  = 2_MiB,
        .usage = gfx::storage_buffer_usage | gfx::indirext_buffer_usage,
    });

    renderer->culled_draw_arguments = device.create_buffer({
        .name  = "Culled Indirect Draw arguments",
        .size  = 2_MiB,
        .usage = gfx::storage_buffer_usage | gfx::indirext_buffer_usage,
    });

    renderer->index_buffer            = UnifiedBufferStorage::create(device, "Unified index buffer", 256_MiB, sizeof(u32), gfx::index_buffer_usage);
    renderer->vertex_positions_buffer = UnifiedBufferStorage::create(device, "Unified positions buffer", 128_MiB, sizeof(float4));
    renderer->vertex_uvs_buffer       = UnifiedBufferStorage::create(device, "Unified uvs buffer", 64_MiB, sizeof(float2));
    renderer->bvh_nodes_buffer        = UnifiedBufferStorage::create(device, "Unified bvh buffer", 256_MiB, sizeof(BVHNode));
    renderer->submeshes_buffer        = UnifiedBufferStorage::create(device, "Unified submeshes buffer", 32_MiB, sizeof(RenderSubMesh));

    renderer->materials_buffer = device.create_buffer({
        .name         = "Materials buffer",
        .size         = 2_MiB,
        .usage        = gfx::storage_buffer_usage,
        .memory_usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
    });

    auto *p_materials = reinterpret_cast<RenderMaterialGPU *>(device.map_buffer(renderer->materials_buffer));
    p_materials[0] = {};

    // Create Render targets
    renderer->settings.resolution_dirty  = true;
    renderer->settings.render_resolution = int2(surface.width, surface.height);

    gfx::DescriptorType one_dynamic_buffer_descriptor = {{{.count = 1, .type = gfx::DescriptorType::DynamicBuffer}}};

    // Create ImGui pass
    imgui_pass_init(device, renderer->imgui_pass, VK_FORMAT_R8G8B8A8_UNORM);

    // Create opaque program
    {
        gfx::GraphicsState state = {};
        state.vertex_shader      = device.create_shader("shaders/opaque.vert.glsl.spv");
        state.fragment_shader    = device.create_shader("shaders/opaque.frag.glsl.spv");
        state.attachments_format
            = {.attachments_format = {VK_FORMAT_R32G32_UINT}, .depth_format = VK_FORMAT_D32_SFLOAT};
        state.descriptors.push_back(one_dynamic_buffer_descriptor);
        renderer->opaque_program = device.create_program("gltf opaque", state);

        gfx::RenderState render_state      = {};
        render_state.depth.test            = VK_COMPARE_OP_GREATER_OR_EQUAL;
        render_state.depth.enable_write    = true;
        render_state.rasterization.culling = false;
        device.compile(renderer->opaque_program, render_state);
    }

    // Create tonemap program
    renderer->taa_program = device.create_program("taa",
                                                 {
                                                     .shader      = device.create_shader("shaders/taa.comp.glsl.spv"),
                                                     .descriptors = {one_dynamic_buffer_descriptor},
                                                 });

    renderer->tonemap_program = device.create_program("tonemap",
                                                     {
                                                         .shader = device.create_shader("shaders/tonemap.comp.glsl.spv"),
                                                         .descriptors = {one_dynamic_buffer_descriptor},
                                                     });

    renderer->path_tracer_program
        = device.create_program("path tracer",
                                {
                                    .shader      = device.create_shader("shaders/path_tracer.comp.glsl.spv"),
                                    .descriptors = {one_dynamic_buffer_descriptor},
                                });

    renderer->instances_culling_program
        = device.create_program("instances culling",
                                {
                                    .shader      = device.create_shader("shaders/instances_culling.comp.glsl.spv"),
                                    .descriptors = {one_dynamic_buffer_descriptor},
                                });

    renderer->parallel_prefix_sum_program
        = device.create_program("parallel prefix sum",
                                {
                                    .shader      = device.create_shader("shaders/parallel_prefix_sum.comp.glsl.spv"),
                                    .descriptors = {one_dynamic_buffer_descriptor},
                                });

    renderer->copy_culled_instances_index_program
        = device.create_program("copy instances",
                                {
                                    .shader      = device.create_shader("shaders/copy_instances_index.comp.glsl.spv"),
                                    .descriptors = {one_dynamic_buffer_descriptor},
                                });

    renderer->init_draw_calls_program
        = device.create_program("init draw calls",
                                {
                                    .shader      = device.create_shader("shaders/init_draw_calls.comp.glsl.spv"),
                                    .descriptors = {one_dynamic_buffer_descriptor},
                                });

    renderer->drawcalls_fill_predicate_program
        = device.create_program("draw calls fill predicate",
                                {
                                    .shader      = device.create_shader("shaders/drawcalls_fill_predicate.comp.glsl.spv"),
                                    .descriptors = {one_dynamic_buffer_descriptor},
                                });

    renderer->copy_draw_calls_program
        = device.create_program("copy culled draw calls",
                                {
                                    .shader      = device.create_shader("shaders/copy_draw_calls.comp.glsl.spv"),
                                    .descriptors = {one_dynamic_buffer_descriptor},
                                });

    renderer->visibility_shading_program
        = device.create_program("visibility shading",
                                {
                                    .shader      = device.create_shader("shaders/visibility_shading.comp.glsl.spv"),
                                    .descriptors = {one_dynamic_buffer_descriptor},
                                });

    auto compute_halton = [](u32 index, u32 radix)
    {
        float result   = 0.f;
        float fraction = 1.f / float(radix);

        while (index != 0)
        {
            result += float(index % radix) * fraction;

            index /= radix;
            fraction /= float(radix);
        }

        return result;
    };

    for (u32 i_halton = 0; i_halton < ARRAY_SIZE(renderer->halton_sequence); i_halton++)
    {
        renderer->halton_sequence[i_halton].x = compute_halton(i_halton + 1, 2);
        renderer->halton_sequence[i_halton].y = compute_halton(i_halton + 1, 3);
    }

    return renderer;
}

Renderer::~Renderer()
{
}

void Renderer::on_resize()
{
    base_renderer->on_resize();
    recreate_framebuffers(*this);
}

void Renderer::reload_shader(std::string_view shader_name)
{
    base_renderer->reload_shader(shader_name);
}

bool Renderer::start_frame()
{
    bool out_of_date_swapchain = base_renderer->start_frame();
    instances_data.start_frame();
    submesh_instances_data.start_frame();
    return out_of_date_swapchain;
}

bool Renderer::end_frame(gfx::ComputeWork &cmd)
{
    bool out_of_date_swapchain = base_renderer->end_frame(cmd);
    if (out_of_date_swapchain)
    {
        return true;
    }

    instances_data.end_frame();
    submesh_instances_data.end_frame();
    return false;
}

void Renderer::display_ui()
{
    ZoneScoped;
    if (auto w = UI::begin_window("Textures"))
    {
        for (uint i = 5; i <= 8; i += 1)
        {
            ImGui::Text("[%u]", i);
            ImGui::Image((void *)((u64)i), float2(256.0f, 256.0f));
        }
    }

    if (auto w = UI::begin_window("Shaders"))
    {
    }

    if (auto w = UI::begin_window("Settings"))
    {
        if (ImGui::CollapsingHeader("Renderer"))
        {
            if (ImGui::SliderFloat("Resolution scale", &settings.resolution_scale, 0.25f, 1.0f))
            {
                settings.resolution_dirty = true;
            }
            if (ImGui::Checkbox("TAA: Clear history", &settings.clear_history))
            {
                first_accumulation_frame = base_renderer->frame_count;
            }
            ImGui::Checkbox("Enable Path tracing", &settings.enable_path_tracing);
            ImGui::Checkbox("Freeze camera culling", &settings.freeze_camera_culling);
            ImGui::Checkbox("Use blue noise", &settings.use_blue_noise);
        }
    }
}

void Renderer::update(const RenderWorld &render_world)
{
    ZoneScoped;

    // -- Handle resize
    if (start_frame())
    {
        on_resize();
        ImGui::EndFrame();
        UI::new_frame();
        return;
    }

    if (settings.resolution_dirty)
    {
        recreate_framebuffers(*this);
        settings.resolution_dirty = false;
    }

    auto &device          = base_renderer->device;
    auto  current_frame   = base_renderer->frame_count % FRAME_QUEUE_LENGTH;
    auto &work_pool       = base_renderer->work_pools[current_frame];
    auto &timings         = base_renderer->timings[current_frame];
    auto  swapchain_image = base_renderer->surface.images[base_renderer->surface.current_image];
    auto &streamer = base_renderer->streamer;

    gfx::GraphicsWork cmd = device.get_graphics_work(work_pool);
    cmd.begin();

    // -- Transfer stuff
    timings.begin_label(cmd, "Uploads");
    cmd.begin_debug_label("Uploads");
    if (base_renderer->frame_count == 0)
    {
        auto & io           = ImGui::GetIO();
        uchar *pixels       = nullptr;
        int    imgui_width  = 0;
        int    imgui_height = 0;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &imgui_width, &imgui_height);
        ASSERT(imgui_width > 0 && imgui_height > 0);
        u32 width  = static_cast<u32>(imgui_width);
        u32 height = static_cast<u32>(imgui_height);
        streamer.upload(imgui_pass.font_atlas, pixels, width * height * sizeof(u32));

        const auto *blue_noise_uuid = "c50c2272-49cf54e5-4ee5e9b2-53a00883";
        auto bn_texture = asset_manager->load_or_import_resource(exo::UUID::from_string(blue_noise_uuid, strlen(blue_noise_uuid)));
        if (!bn_texture)
        {
        }
        else
        {
            Texture *texture = dynamic_cast<Texture*>(bn_texture.value());
            ASSERT(texture);
            blue_noise = device.create_image({
                .name       = "Blue noise",
                .size       = int3(texture->width, texture->height, texture->depth),
                .mip_levels = static_cast<u32>(texture->levels),
                .format     = to_vk(texture->format),
            });
            streamer.upload(blue_noise, *texture);
        }
    }
    streamer.update(work_pool);
    cmd.end_debug_label();
    timings.end_label(cmd);

    // -- Get geometry from the scene and prepare the draw commands
    timings.begin_label(cmd, "prepare geometry");
    this->prepare_geometry(render_world);
    timings.end_label(cmd);

    // -- Update global data
    float2 current_sample = halton_sequence[base_renderer->frame_count % 16] - float2(0.5);

    static float4x4 last_view = render_world.main_camera_view;
    static float4x4 last_proj = render_world.main_camera_projection;

    auto *global_data                       = base_renderer->bind_global_options<GlobalUniform>();
    global_data->camera_view                = render_world.main_camera_view;
    global_data->camera_projection          = render_world.main_camera_projection;
    global_data->camera_view_inverse        = render_world.main_camera_view_inverse;
    global_data->camera_projection_inverse  = render_world.main_camera_projection_inverse;
    global_data->camera_previous_view       = last_view;
    global_data->camera_previous_projection = last_proj;
    global_data->render_resolution          = floor(settings.resolution_scale * float2(settings.render_resolution)),
    global_data->jitter_offset              = current_sample;
    global_data->delta_t                    = 0.016f;
    global_data->frame_count                = base_renderer->frame_count;
    global_data->first_accumulation_frame   = this->first_accumulation_frame;
    global_data->meshes_data_descriptor     = device.get_buffer_storage_index(meshes_buffer);
    global_data->instances_data_descriptor  = device.get_buffer_storage_index(instances_data.buffer);
    global_data->instances_offset           = this->instances_offset;
    global_data->submesh_instances_data_descriptor = device.get_buffer_storage_index(submesh_instances_data.buffer);
    global_data->submesh_instances_offset          = this->submesh_instances_offset;
    global_data->tlas_descriptor                   = device.get_buffer_storage_index(tlas_buffer);
    global_data->submesh_instances_count           = static_cast<u32>(this->submesh_instances.size());
    global_data->index_buffer_descriptor           = device.get_buffer_storage_index(this->index_buffer.buffer);
    global_data->vertex_positions_descriptor = device.get_buffer_storage_index(this->vertex_positions_buffer.buffer);
    global_data->bvh_nodes_descriptor        = device.get_buffer_storage_index(this->bvh_nodes_buffer.buffer);
    global_data->submeshes_descriptor        = device.get_buffer_storage_index(this->submeshes_buffer.buffer);
    global_data->culled_instances_indices_descriptor = device.get_buffer_storage_index(culled_instances_compact_indices);
    global_data->materials_descriptor  = device.get_buffer_storage_index(this->materials_buffer);
    global_data->vertex_uvs_descriptor = device.get_buffer_storage_index(this->vertex_uvs_buffer.buffer);

    last_view = render_world.main_camera_view;
    last_proj = render_world.main_camera_projection;

    device.update_globals();

    // -- Do the actual rendering

    cmd.bind_global_set();
    cmd.bind_index_buffer(this->index_buffer.buffer, VK_INDEX_TYPE_UINT32);

    // vulkan only: this command buffer will wait for the image acquire semaphore
    cmd.wait_for_acquired(base_renderer->surface, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

    {
        float2 scaled_resolution = floor(settings.resolution_scale * float2(settings.render_resolution));

        VkViewport viewport{};
        viewport.width    = scaled_resolution.x;
        viewport.height   = scaled_resolution.y;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        cmd.set_viewport(viewport);

        VkRect2D scissor      = {};
        scissor.extent.width  = static_cast<u32>(scaled_resolution.x);
        scissor.extent.height = static_cast<u32>(scaled_resolution.y);
        cmd.set_scissor(scissor);
    }

    // Opaque pass
    if (settings.enable_path_tracing)
    {
        timings.begin_label(cmd, "Path tracing");
        ZoneNamedN(path_tracing, "Pathtracing", true);
        cmd.begin_debug_label("Path tracing");
        cmd.barrier(hdr_buffer, gfx::ImageUsage::ComputeShaderReadWrite);

        struct PathTracerOptions
        {
            u32 storage_output;
        };

        auto *options           = base_renderer->bind_shader_options<PathTracerOptions>(cmd, path_tracer_program);
        options->storage_output = device.get_image_storage_index(hdr_buffer);

        auto hdr_buffer_size = device.get_image_size(hdr_buffer);

        cmd.bind_pipeline(path_tracer_program);
        cmd.dispatch(dispatch_size(hdr_buffer_size, 16));
        cmd.end_debug_label();
        timings.end_label(cmd);
    }
    else
    {
        ZoneNamedN(render_opaque, "Render Opaque", true);
        // Instances culling
        {
            timings.begin_label(cmd, "GPU Culling");
            cmd.begin_debug_label("GPU Culling");
            i32 submesh_instances_to_cull = static_cast<i32>(submesh_instances.size());

            // Prepare draw calls with instance count = 0
            cmd.barrier(draw_arguments, gfx::BufferUsage::ComputeShaderReadWrite);
            struct GenDrawCallOptions
            {
                u32 draw_arguments_descriptor;
            };
            {
                auto *options = base_renderer->bind_shader_options<GenDrawCallOptions>(cmd, init_draw_calls_program);
                options->draw_arguments_descriptor = device.get_buffer_storage_index(draw_arguments);
            }
            cmd.bind_pipeline(init_draw_calls_program);
            cmd.dispatch(dispatch_size({static_cast<i32>(this->draw_count), 1, 1}, 32));

            // Frustum culling
            static float4x4 culling_view = render_world.main_camera_view;
            if (settings.freeze_camera_culling == false)
            {
                culling_view = render_world.main_camera_view;
            }
            cmd.barrier(predicate_buffer, gfx::BufferUsage::TransferDst);
            cmd.fill_buffer(predicate_buffer, 0);
            cmd.barrier(predicate_buffer, gfx::BufferUsage::ComputeShaderReadWrite);
            cmd.barrier(predicate_buffer, gfx::BufferUsage::ComputeShaderReadWrite);
            struct CullInstancesOptions
            {
                float4x4 camera_view;
                u32      instances_visibility_descriptor;
            };
            auto *options = base_renderer->bind_shader_options<CullInstancesOptions>(cmd, instances_culling_program);
            options->camera_view                     = culling_view;
            options->instances_visibility_descriptor = device.get_buffer_storage_index(predicate_buffer);
            cmd.bind_pipeline(instances_culling_program);
            cmd.dispatch(dispatch_size({submesh_instances_to_cull, 1, 1}, 32));

            // Copy culled instances indices
            cmd.barrier(draw_arguments, gfx::BufferUsage::ComputeShaderReadWrite);
            struct CopyInstancesOptions
            {
                u32 predicate_descriptor;
                u32 scanned_indices_descriptor;
                u32 reduction_group_sum_descriptor;
                u32 instances_index_descriptor;
                u32 draw_arguments_descriptor;
            };
            CopyInstancesOptions copy_options           = {};
            copy_options.predicate_descriptor           = device.get_buffer_storage_index(predicate_buffer);
            copy_options.scanned_indices_descriptor     = device.get_buffer_storage_index(scanned_indices);
            copy_options.reduction_group_sum_descriptor = device.get_buffer_storage_index(group_sum_reduction);
            copy_options.instances_index_descriptor = device.get_buffer_storage_index(culled_instances_compact_indices);
            copy_options.draw_arguments_descriptor  = device.get_buffer_storage_index(draw_arguments);
            this->compact_buffer(cmd,
                                 submesh_instances_to_cull,
                                 copy_culled_instances_index_program,
                                 &copy_options,
                                 sizeof(CopyInstancesOptions));
            cmd.end_debug_label();
            timings.end_label(cmd);
        }

        // Compact draw calls
        {
            timings.begin_label(cmd, "Compact draw calls");
            cmd.begin_debug_label("Compact draw calls");
            // Fill draw calls predicate buffer
            cmd.barrier(predicate_buffer, gfx::BufferUsage::TransferDst);
            cmd.fill_buffer(predicate_buffer, 0);
            cmd.barrier(predicate_buffer, gfx::BufferUsage::ComputeShaderReadWrite);

            cmd.barrier(predicate_buffer, gfx::BufferUsage::ComputeShaderReadWrite);
            struct FillPredicateOptions
            {
                u32 predicate_descriptor;
                u32 draw_arguments_descriptor;
            };
            auto *options
                = base_renderer->bind_shader_options<FillPredicateOptions>(cmd, drawcalls_fill_predicate_program);
            options->predicate_descriptor      = device.get_buffer_storage_index(predicate_buffer);
            options->draw_arguments_descriptor = device.get_buffer_storage_index(draw_arguments);
            cmd.bind_pipeline(drawcalls_fill_predicate_program);
            cmd.dispatch(dispatch_size({static_cast<i32>(this->draw_count), 1, 1}, 32));

            cmd.barrier(draw_arguments, gfx::BufferUsage::ComputeShaderReadWrite);
            cmd.barrier(culled_draw_arguments, gfx::BufferUsage::ComputeShaderReadWrite);
            struct CopyDrawcallsOptions
            {
                u32 predicate_descriptor;
                u32 scanned_indices_descriptor;
                u32 reduction_group_sum_descriptor;
                u32 draw_arguments_descriptor;
                u32 culled_draw_arguments_descriptor;
            };
            CopyDrawcallsOptions copy_options             = {};
            copy_options.predicate_descriptor             = device.get_buffer_storage_index(predicate_buffer);
            copy_options.scanned_indices_descriptor       = device.get_buffer_storage_index(scanned_indices);
            copy_options.reduction_group_sum_descriptor   = device.get_buffer_storage_index(group_sum_reduction);
            copy_options.draw_arguments_descriptor        = device.get_buffer_storage_index(draw_arguments);
            copy_options.culled_draw_arguments_descriptor = device.get_buffer_storage_index(culled_draw_arguments);
            this->compact_buffer(cmd,
                                 static_cast<i32>(this->draw_count),
                                 copy_draw_calls_program,
                                 &copy_options,
                                 sizeof(CopyDrawcallsOptions));
            cmd.end_debug_label();
            timings.end_label(cmd);
        }

        // Fill visibility
        timings.begin_label(cmd, "Fill visibility buffer");
        cmd.begin_debug_label("Fill visibility buffer");
        cmd.clear_barrier(visibility_buffer, gfx::ImageUsage::ColorAttachment);
        cmd.clear_barrier(depth_buffer, gfx::ImageUsage::DepthAttachment);
        cmd.begin_pass(visibility_depth_fb,
                       std::array{gfx::LoadOp::clear({.color = {.float32 = {0.0f, 0.0f, 0.0f, 0.0f}}}),
                                  gfx::LoadOp::clear({.depthStencil = {.depth = 0.0f}})});

        struct OpaqueOptions
        {
            u32 nothing;
        };
        {
            auto *options    = base_renderer->bind_shader_options<OpaqueOptions>(cmd, opaque_program);
            options->nothing = 42;
            cmd.bind_pipeline(opaque_program, 0);
            cmd.draw_indexed_indirect_count({.arguments_buffer = culled_draw_arguments,
                                             .arguments_offset = sizeof(u32),
                                             .count_buffer     = culled_draw_arguments,
                                             .max_draw_count   = this->draw_count});
        }
        cmd.end_pass();
        cmd.end_debug_label();
        timings.end_label(cmd);

        // Deferred shading
        timings.begin_label(cmd, "Visibility buffer shading");
        cmd.begin_debug_label("Visibility buffer shading");
        struct ShadingOptions
        {
            u32 sampled_visibility_buffer;
            u32 sampled_depth_buffer;
            u32 sampled_blue_noise;
            u32 storage_hdr_buffer;
        };
        cmd.barrier(visibility_buffer, gfx::ImageUsage::ComputeShaderRead);
        cmd.barrier(depth_buffer, gfx::ImageUsage::ComputeShaderRead);
        cmd.barrier(hdr_buffer, gfx::ImageUsage::ComputeShaderReadWrite);
        auto hdr_buffer_size = device.get_image_size(hdr_buffer);
        {
            auto *options = base_renderer->bind_shader_options<ShadingOptions>(cmd, visibility_shading_program);
            options->sampled_visibility_buffer = device.get_image_sampled_index(visibility_buffer);
            options->sampled_depth_buffer      = device.get_image_sampled_index(depth_buffer);
            options->sampled_blue_noise
                = settings.use_blue_noise ? device.get_image_sampled_index(blue_noise) : u32_invalid;
            options->storage_hdr_buffer = device.get_image_storage_index(hdr_buffer);
            cmd.bind_pipeline(visibility_shading_program);
            cmd.dispatch(dispatch_size(hdr_buffer_size, 16));
        }
        cmd.end_debug_label();
        timings.end_label(cmd);
    }

    auto &current_history  = history_buffers[current_frame % 2];
    auto &previous_history = history_buffers[(current_frame + 1) % 2];

    // TAA
    {
        ZoneNamedN(taa, "TAA", true);
        timings.begin_label(cmd, "TAA");
        cmd.begin_debug_label("TAA");

        if (settings.clear_history)
        {
            cmd.clear_barrier(previous_history, gfx::ImageUsage::TransferDst);
            cmd.clear_image(previous_history, {.float32 = {0.0f, 0.0f, 0.0f, 0.0f}});
            this->first_accumulation_frame = base_renderer->frame_count;
        }

        cmd.barrier(hdr_buffer, gfx::ImageUsage::ComputeShaderRead);
        cmd.barrier(previous_history, gfx::ImageUsage::ComputeShaderRead);
        cmd.clear_barrier(current_history, gfx::ImageUsage::ComputeShaderReadWrite);

        auto history_size = device.get_image_size(current_history);

        struct TAAOptions
        {
            uint sampled_hdr_buffer;
            uint sampled_depth_buffer;
            uint sampled_previous_history;
            uint storage_current_history;
        };

        auto *options                     = base_renderer->bind_shader_options<TAAOptions>(cmd, taa_program);
        options->sampled_hdr_buffer       = device.get_image_sampled_index(hdr_buffer);
        options->sampled_depth_buffer     = device.get_image_sampled_index(depth_buffer);
        options->sampled_previous_history = device.get_image_sampled_index(previous_history);
        options->storage_current_history  = device.get_image_storage_index(current_history);
        cmd.bind_pipeline(taa_program);
        cmd.dispatch(dispatch_size(history_size, 16));
        cmd.end_debug_label();
        timings.end_label(cmd);
    }

    // Tonemap
    auto tonemap_input = current_history;
    {
        ZoneNamedN(tonemap, "Tonemap", true);
        timings.begin_label(cmd, "Tonemap");
        cmd.begin_debug_label("Tonemap");
        cmd.barrier(tonemap_input, gfx::ImageUsage::ComputeShaderRead);
        cmd.clear_barrier(ldr_buffer, gfx::ImageUsage::ComputeShaderReadWrite);

        auto input_size = device.get_image_size(tonemap_input);

        struct TonemapOptions
        {
            uint sampled_input;
            uint storage_output_frame;
        };

        auto *options                 = base_renderer->bind_shader_options<TonemapOptions>(cmd, tonemap_program);
        options->sampled_input        = device.get_image_sampled_index(tonemap_input);
        options->storage_output_frame = device.get_image_storage_index(ldr_buffer);
        cmd.bind_pipeline(tonemap_program);
        cmd.dispatch(dispatch_size(input_size, 16));
        cmd.end_debug_label();
        timings.end_label(cmd);
    }

    // ImGui pass
    ImGui::Render();
    if (streamer.is_uploaded(imgui_pass.font_atlas))
    {
        imgui_pass_draw(*base_renderer, imgui_pass, cmd, ldr_fb);
    }
    UI::new_frame();

    timings.begin_label(cmd, "Present + final blit");

    cmd.barrier(ldr_buffer, gfx::ImageUsage::TransferSrc);
    cmd.clear_barrier(swapchain_image, gfx::ImageUsage::TransferDst);
    cmd.blit_image(ldr_buffer, swapchain_image);
    cmd.barrier(swapchain_image, gfx::ImageUsage::Present);

    timings.end_label(cmd);
    cmd.end();

    if (end_frame(cmd))
    {
        on_resize();
        return;
    }
}

void Renderer::prepare_geometry(const RenderWorld &render_world)
{
    ZoneScoped;

    auto &device = base_renderer->device;

    // -- Clear state from previous frame
    {
        ZoneScopedN("Clear render state");
        draw_count = 0;
        for (auto &[mesh_uuid, mesh_instance] : mesh_instances)
        {
            mesh_instance.clear();
        }
        render_instances.clear();
        submesh_instances.clear();
    }

    // -- Gather all instances per uploaded mesh
    for (u32 i_drawable = 0; i_drawable < render_world.drawable_instances.size(); i_drawable += 1)
    {
        ZoneScopedN("Gather drawables");
        const auto &drawable = render_world.drawable_instances[i_drawable];
        if (is_mesh_uploaded(*this, drawable.mesh_asset))
        {
            mesh_instances[drawable.mesh_asset].push_back(i_drawable);
            continue;
        }

        upload_mesh(*this, drawable.mesh_asset);
    }

    // -- Generate a list of render instances and instances submesh from the uploaded meshes' instances
    for (auto &[mesh_uuid, mesh_instance] : mesh_instances)
    {
        ZoneScopedN("Generate render instances lists");
        if (mesh_instance.empty())
        {
            continue;
        }

        const auto &mesh_asset = *dynamic_cast<Mesh *>(asset_manager->get_asset(mesh_uuid).value());
        ASSERT(uploaded_meshes.contains(mesh_asset.uuid) && uploaded_meshes[mesh_asset.uuid].is_valid());

        auto render_mesh_handle = uploaded_meshes[mesh_asset.uuid];
        auto& render_mesh = *render_meshes.get(render_mesh_handle);

        render_mesh.first_instance = static_cast<u32>(render_instances.size());

        for (auto i_drawable : mesh_instance)
        {
            const auto &drawable = render_world.drawable_instances[i_drawable];

            u32 i_instance = static_cast<u32>(render_instances.size());
            render_instances.push_back({
                .object_to_world = drawable.world_transform,
                .world_to_object = inverse_transform(drawable.world_transform),
                .i_render_mesh   = render_mesh_handle.value(),
            });

            for (u32 i_submesh = 0; i_submesh < mesh_asset.submeshes.size(); i_submesh += 1)
            {
                submesh_instances.push_back({
                    .i_mesh     = render_mesh_handle.value(),
                    .i_submesh  = i_submesh,
                    .i_instance = i_instance,
                    .i_draw     = draw_count + i_submesh,
                });
            }

            draw_count += static_cast<u32>(mesh_asset.submeshes.size());
        }
    }

    // Build and upload the TLAS
    static BVH tlas = {};
    {
        ZoneScopedN("Build TLAS");
        static Vec<BVHNode>  roots;
        static Vec<float4x4> transforms;
        static Vec<u32>      indices;
        roots.clear();
        transforms.clear();
        indices.clear();
        roots.resize(render_instances.size());
        transforms.resize(render_instances.size());
        indices.resize(render_instances.size());
        for (u32 i_instance = 0; i_instance < render_instances.size(); i_instance += 1)
        {
            const auto &render_instance = render_instances[i_instance];
            const auto &render_mesh     = render_meshes.get_unchecked(render_instance.i_render_mesh);

            roots[i_instance]      = render_mesh.bvh_root;
            transforms[i_instance] = render_instance.object_to_world;
            indices[i_instance]    = i_instance;
        }

        static BVHScratchMemory scratch_bvh = {};
        create_tlas(scratch_bvh, tlas, roots, transforms, indices);
    }

    {
        // Upload all data to draw
        ZoneScopedN("Upoad instances");
        auto *tlas_gpu = reinterpret_cast<BVHNode *>(device.map_buffer(tlas_buffer));
        ASSERT(tlas.nodes.size() * sizeof(BVHNode) < 32_MiB);
        std::memcpy(tlas_gpu, tlas.nodes.data(), tlas.nodes.size() * sizeof(BVHNode));

        auto [p_instances, instance_offset] = instances_data.allocate(device, render_instances.size() * sizeof(RenderInstance));
        std::memcpy(p_instances, render_instances.data(), render_instances.size() * sizeof(RenderInstance));
        this->instances_offset = static_cast<u32>(instance_offset / sizeof(RenderInstance));

        usize submesh_instances_size = submesh_instances.size() * sizeof(SubMeshInstance);
        auto [p_submesh_instances, submesh_instance_offset] = submesh_instances_data.allocate(device, submesh_instances_size);
        std::memcpy(p_submesh_instances, submesh_instances.data(), submesh_instances_size);
        this->submesh_instances_offset = static_cast<u32>(submesh_instance_offset / sizeof(SubMeshInstance));
    }

    ImGui::Text("Renderer draw count: %u", draw_count);
    ImGui::Text("Uploaded meshes: %zu (%u)", uploaded_meshes.size(), render_meshes.size);
    ImGui::Text("Renderer instances: %zu", render_instances.size());
    ImGui::Text("Renderer submesh instances: %zu", submesh_instances.size());
}

void Renderer::compact_buffer(gfx::ComputeWork &cmd, i32 count, Handle<gfx::ComputeProgram> copy_program,
                              const void *options_data, usize options_len)
{
    ZoneScoped;
    auto &device = base_renderer->device;

    cmd.begin_debug_label("Buffer reduction");

    ASSERT(count < 128 * 128);

    // clear buffers
    cmd.barrier(group_sum_reduction, gfx::BufferUsage::TransferDst);
    cmd.barrier(scanned_indices, gfx::BufferUsage::TransferDst);
    cmd.fill_buffer(group_sum_reduction, 0);
    cmd.fill_buffer(scanned_indices, 0);
    cmd.barrier(group_sum_reduction, gfx::BufferUsage::ComputeShaderReadWrite);
    cmd.barrier(scanned_indices, gfx::BufferUsage::ComputeShaderReadWrite);

    // Scan predicate buffer
    {
        cmd.barrier(predicate_buffer, gfx::BufferUsage::ComputeShaderRead);
        cmd.barrier(scanned_indices, gfx::BufferUsage::ComputeShaderReadWrite);
        cmd.barrier(group_sum_reduction, gfx::BufferUsage::ComputeShaderReadWrite);
        struct ScanOptions
        {
            u32 input_descriptor;
            u32 output_descriptor;
            u32 reduction_group_sum_descriptor;
        };

        auto *options              = base_renderer->bind_shader_options<ScanOptions>(cmd, parallel_prefix_sum_program);
        options->input_descriptor  = device.get_buffer_storage_index(predicate_buffer);
        options->output_descriptor = device.get_buffer_storage_index(scanned_indices);
        options->reduction_group_sum_descriptor = device.get_buffer_storage_index(group_sum_reduction);

        cmd.bind_pipeline(parallel_prefix_sum_program);
        cmd.dispatch(dispatch_size({count, 1, 1}, 128));
    }

    // Scan group sums
    {
        cmd.barrier(group_sum_reduction, gfx::BufferUsage::ComputeShaderReadWrite);
        struct ScanOptions
        {
            u32 input_descriptor;
            u32 output_descriptor;
            u32 reduction_group_sum_descriptor;
        };

        auto *options              = base_renderer->bind_shader_options<ScanOptions>(cmd, parallel_prefix_sum_program);
        options->input_descriptor  = device.get_buffer_storage_index(group_sum_reduction);
        options->output_descriptor = options->input_descriptor;
        options->reduction_group_sum_descriptor = u32_invalid;

        cmd.bind_pipeline(parallel_prefix_sum_program);
        cmd.dispatch({1, 1, 1}); // 128 groups seems plenty (128*128 = 16k elements)
    }

    // Copy elements that match predicate
    {
        cmd.barrier(scanned_indices, gfx::BufferUsage::ComputeShaderRead);
        cmd.barrier(group_sum_reduction, gfx::BufferUsage::ComputeShaderRead);

        auto [options, options_offset] = base_renderer->dynamic_uniform_buffer.allocate(device, options_len);
        std::memcpy(options, options_data, options_len);
        cmd.bind_uniform_buffer(copy_program,
                                0,
                                base_renderer->dynamic_uniform_buffer.buffer,
                                options_offset,
                                options_len);

        cmd.bind_pipeline(copy_program);
        cmd.dispatch(dispatch_size({count, 1, 1}, 128));
    }
    cmd.end_debug_label();
}
