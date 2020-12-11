#include "render/renderer.hpp"

#include "camera.hpp"
#include "components/camera_component.hpp"
#include "components/input_camera_component.hpp"
#include "components/sky_atmosphere_component.hpp"
#include "components/transform_component.hpp"
#include "gltf.hpp"
#include "timer.hpp"
#include "tools.hpp"
#include "ui.hpp"

#include <cstring> // for memset
#include <future>
#include <imgui/imgui.h>
#include <random>
#include <vulkan/vulkan_core.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace my_app
{

// frame data
void Renderer::create(Renderer &r, const platform::Window &window, TimerData &timer)
{
    // where to put this code?
    //

    vulkan::API::create(r.api, window);
    RenderGraph::create(r.graph, r.api);
    r.model = std::make_shared<Model>(load_model("../models/Sponza/glTF/Sponza.gltf")); // TODO: where??

    r.p_timer  = &timer;

    r.api.global_bindings.binding({
        .set    = vulkan::GLOBAL_DESCRIPTOR_SET,
        .slot   = 0,
        .stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
        .type   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
    });

    // bindless binding model, most textures are bound only once per frame
    // - per frame/pass textures are hardcoded at the beginning of the set (immediates are 20 bit on AMD)
    // - per draw textures are accessed with (push constant index + 1)
    // (https://gpuopen.com/wp-content/uploads/2016/03/VulkanFastPaths.pdf)
    r.api.global_bindings.binding({
        .set    = vulkan::GLOBAL_DESCRIPTOR_SET,
        .slot   = 1,
        .stages = VK_SHADER_STAGE_FRAGMENT_BIT,
        .type   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .count  = 128,
    });

    r.api.create_global_set();

    r.imgui              = create_imgui_pass(r.api);
    r.checkerboard_floor = create_floor_pass(r.api);
    r.procedural_sky     = create_procedural_sky_pass(r.api);
    r.tonemapping        = create_tonemapping_pass(r.api);
    r.gltf               = create_gltf_pass(r.api, r.model);
    r.voxels             = create_voxel_pass(r.api);
    r.luminance          = create_luminance_pass(r.api);

    // basic resources

    r.settings            = {};
    r.settings.render_resolution = {.x = r.api.ctx.swapchain.extent.width, .y = r.api.ctx.swapchain.extent.height};

    r.graph.on_resize(r.settings.render_resolution.x * r.settings.resolution_scale, r.settings.render_resolution.y * r.settings.resolution_scale);

    r.depth_buffer = r.graph.image_descs.add({.name = "Depth Buffer", .format = VK_FORMAT_D32_SFLOAT});
    r.hdr_buffer = r.graph.image_descs.add({.name = "HDR Buffer", .format = VK_FORMAT_R16G16B16A16_SFLOAT});
    r.ldr_buffer = r.graph.image_descs.add({.name = "LDR Buffer", .format = r.api.ctx.swapchain.format.format});

    r.trilinear_sampler = r.api.create_sampler({
        .mag_filter   = VK_FILTER_LINEAR,
        .min_filter   = VK_FILTER_LINEAR,
        .mip_map_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .address_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    });

    r.nearest_sampler = r.api.create_sampler({
        .mag_filter   = VK_FILTER_NEAREST,
        .min_filter   = VK_FILTER_NEAREST,
        .mip_map_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .address_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    });

    r.random_rotations = r.api.create_image({
        .name                = "Random rotations",
        .type                = VK_IMAGE_TYPE_3D,
        .format              = VK_FORMAT_R32G32_SFLOAT,
        .width               = 32,
        .height              = 32,
        .depth               = 32,
    });

    constexpr usize rotations_count = 32*32*32;
    std::array<float2, rotations_count> rotations;

    std::random_device rd;  //Will be used to obtain a seed for the random number engine
    std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
    std::uniform_real_distribution<> dis(0.0, 1.0);
    for (auto &rotation : rotations)
    {
        auto angle = dis(gen) * 2.0f * PI;
        rotation = float2(std::cos(angle), std::sin(angle));
    }

    r.api.upload_image(r.random_rotations, rotations.data(), sizeof(rotations));
    r.api.transfer_done(r.random_rotations);

    r.sun.pitch    = 0.0f;
    r.sun.yaw      = 25.0f;
    r.sun.roll     = 80.0f;

    // it would be nice to be able to create those in the create_procedural_sky_pass function

    r.transmittance_lut = r.graph.image_descs.add({
        .name      = "Transmittance LUT",
        .size_type = SizeType::Absolute,
        .size      = float3(256, 64, 1),
        .format    = VK_FORMAT_R16G16B16A16_SFLOAT,
    });

    r.skyview_lut = r.graph.image_descs.add({
        .name      = "Skyview LUT",
        .size_type = SizeType::Absolute,
        .size      = float3(192, 108, 1),
        .format    = VK_FORMAT_R16G16B16A16_SFLOAT,
    });

    r.multiscattering_lut = r.graph.image_descs.add({
        .name      = "Multiscattering LUT",
        .size_type = SizeType::Absolute,
        .size      = float3(32, 32, 1),
        .format    = VK_FORMAT_R16G16B16A16_SFLOAT,
    });

    r.voxels_albedo = r.graph.image_descs.add({
        .name          = "Voxels albedo",
        .size_type     = SizeType::Absolute,
        .size          = float3(r.voxel_options.res),
        .type          = VK_IMAGE_TYPE_3D,
        .format        = VK_FORMAT_R8G8B8A8_UNORM,
        .extra_formats = {VK_FORMAT_R32_UINT},
    });

    r.voxels_normal = r.graph.image_descs.add({
        .name          = "Voxels normal",
        .size_type     = SizeType::Absolute,
        .size          = float3(r.voxel_options.res),
        .type          = VK_IMAGE_TYPE_3D,
        .format        = VK_FORMAT_R8G8B8A8_UNORM,
        .extra_formats = {VK_FORMAT_R32_UINT},
    });

    r.voxels_radiance = r.graph.image_descs.add({
        .name      = "Voxels radiance",
        .size_type = SizeType::Absolute,
        .size      = float3(r.voxel_options.res),
        .type      = VK_IMAGE_TYPE_3D,
        .format    = VK_FORMAT_R16G16B16A16_SFLOAT,
    });

    r.average_luminance = r.graph.image_descs.add({
        .name          = "Average luminance",
        .size_type     = SizeType::Absolute,
        .size          = float3(1.0f),
        .type          = VK_IMAGE_TYPE_2D,
        .format        = VK_FORMAT_R32_SFLOAT,
    });


    usize name_i     = 0;
    std::array names = {
        "Voxels volume -X",
        "Voxels volume +X",
        "Voxels volume -Y",
        "Voxels volume +Y",
        "Voxels volume -Z",
        "Voxels volume +Z",
    };

    for (auto &volume : r.voxels_directional_volumes)
    {
        u32 size = r.voxel_options.res / 2;
        volume   = r.graph.image_descs.add({
            .name      = names[name_i++],
            .size_type = SizeType::Absolute,
            .size      = float3(size),
            .type      = VK_IMAGE_TYPE_3D,
            .format    = VK_FORMAT_R16G16B16A16_SFLOAT,
            .levels    = static_cast<u32>(std::floor(std::log2(size)) + 1.0),
        });
    }

    r.shadow_cascades.resize(r.settings.shadow_cascades_count);
    for (auto &shadow_cascade : r.shadow_cascades)
    {
        shadow_cascade = r.graph.image_descs.add({
            .name      = "Shadow cascade",
            .size_type = SizeType::Absolute,
            .size      = float3(2048.0f, 2048.0f, 1.0f),
            .type      = VK_IMAGE_TYPE_2D,
            .format    = VK_FORMAT_D32_SFLOAT,
        });
    }
}

void Renderer::destroy()
{
    api.wait_idle();
    graph.destroy();
    api.destroy();
}

void Renderer::on_resize(int window_width, int window_height)
{
    // resize swapchain
    api.on_resize(window_width, window_height);
}

void Renderer::wait_idle() const { api.wait_idle(); }

void Renderer::reload_shader(std::string_view /*unused*/) {}

/// --- glTF model pass

Renderer::GltfPass create_gltf_pass(vulkan::API &api, std::shared_ptr<Model> &_model)
{
    Renderer::GltfPass pass;
    pass.model = _model;

    auto &model = *pass.model;

    /// --- TODO Move

    model.nodes_preorder.clear();
    model.nodes_preorder.reserve(model.nodes.size());

    model.cached_transforms.resize(model.nodes.size());

    std::vector<u32> nodes_stack;
    nodes_stack.reserve(model.nodes.size());
    nodes_stack.push_back(model.scene[0]);

    while (!nodes_stack.empty())
    {
        auto node_idx = nodes_stack.back();
        nodes_stack.pop_back();
        auto &node = model.nodes[node_idx];

        node.dirty                        = false;
        auto translation                  = float4x4::identity(); // glm::translate(glm::mat4(1.0f), node.translation);
        auto rotation                     = float4x4::identity(); // glm::mat4(node.rotation);
        auto scale                        = float4x4::identity(); // assume uniform scale
        scale.at(0, 0)                    = node.scale.x;
        scale.at(1, 1)                    = node.scale.y;
        scale.at(2, 2)                    = node.scale.z;
        model.cached_transforms[node_idx] = translation * rotation * scale;

        model.nodes_preorder.push_back(node_idx);

        for (auto child : node.children)
        {
            nodes_stack.push_back(child);
        }
    }

    /// -- end TODO

    usize vbuffer_size = model.vertices.size() * sizeof(GltfVertex);
    pass.vertex_buffer = api.create_buffer({
        .name  = "glTF Vertex Buffer",
        .size  = vbuffer_size,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    });
    api.upload_buffer(pass.vertex_buffer, model.vertices.data(), vbuffer_size);

    usize ibuffer_size = model.indices.size() * sizeof(u16);
    pass.index_buffer  = api.create_buffer({
        .name  = "glTF Index Buffer",
        .size  = ibuffer_size,
        .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    });
    api.upload_buffer(pass.index_buffer, model.indices.data(), ibuffer_size);

    /// --- Create samplers
    for (auto &sampler : model.samplers)
    {
        vulkan::SamplerInfo sinfo;

        switch (sampler.mag_filter)
        {
            case Filter::Nearest:
                sinfo.mag_filter = VK_FILTER_NEAREST;
                break;
            case Filter::Linear:
                sinfo.mag_filter = VK_FILTER_LINEAR;
                break;
            default:
                break;
        }

        switch (sampler.min_filter)
        {
            case Filter::Nearest:
                sinfo.min_filter = VK_FILTER_NEAREST;
                break;
            case Filter::Linear:
                sinfo.min_filter = VK_FILTER_LINEAR;
                break;
            default:
                break;
        }

        switch (sampler.wrap_s)
        {
            case Wrap::Repeat:
                sinfo.address_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
                break;
            case Wrap::ClampToEdge:
                sinfo.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
                break;
            case Wrap::MirroredRepeat:
                sinfo.address_mode = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
                break;
            default:
                break;
        }

        pass.samplers.push_back(api.create_sampler(sinfo));
    }

    /// --- Create images
    struct GltfImageInfo
    {
        int width;
        int height;
        u8 *pixels;
        int nb_comp;
        VkFormat format;
    };
    std::vector<std::future<GltfImageInfo>> images_pixels;
    images_pixels.resize(model.images.size());

    for (usize image_i = 0; image_i < model.images.size(); image_i++)
    {
        const auto &image      = model.images[image_i];
        images_pixels[image_i] = std::async(std::launch::async, [&]() {
            GltfImageInfo info = {};
            info.pixels        = stbi_load_from_memory(image.data.data(),
                                                static_cast<int>(image.data.size()),
                                                &info.width,
                                                &info.height,
                                                &info.nb_comp,
                                                0);

            if (info.nb_comp == 1)
            { // NOLINT
                info.format = VK_FORMAT_R8_UNORM;
            }
            else if (info.nb_comp == 2)
            { // NOLINT
                info.format = VK_FORMAT_R8G8_UNORM;
            }
            else if (info.nb_comp == 3)
            { // NOLINT
                stbi_image_free(info.pixels);
                int wanted_nb_comp = 4;
                info.pixels        = stbi_load_from_memory(image.data.data(),
                                                    static_cast<int>(image.data.size()),
                                                    &info.width,
                                                    &info.height,
                                                    &info.nb_comp,
                                                    wanted_nb_comp);
                info.format        = image.srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
                info.nb_comp       = wanted_nb_comp;
            }
            else if (info.nb_comp == 4)
            { // NOLINT
                info.format = image.srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
            }
            else
            { // NOLINT
                assert(false);
            }

            return info;
        });
    }

    for (usize image_i = 0; image_i < model.images.size(); image_i++)
    {
        auto image_info = images_pixels[image_i].get();

        vulkan::ImageInfo iinfo;
        iinfo.name                = "glTF image";
        iinfo.width               = static_cast<u32>(image_info.width);
        iinfo.height              = static_cast<u32>(image_info.height);
        iinfo.depth               = 1;
        iinfo.format              = image_info.format;
        iinfo.generate_mip_levels = true;

        auto image_h = api.create_image(iinfo);
        pass.images.push_back(image_h);

        auto size = static_cast<usize>(image_info.width * image_info.height * image_info.nb_comp);
        api.upload_image(image_h, image_info.pixels, size);
        api.generate_mipmaps(image_h);
        api.transfer_done(image_h);

        stbi_image_free(image_info.pixels);
    }

    /// --- Create programs
    vulkan::DepthState depth_state = {
        .test         = VK_COMPARE_OP_EQUAL,
        .enable_write = false,
    };

    pass.shading = api.create_program({
        .vertex_shader   = api.create_shader("shaders/gltf.vert.spv"),
        .fragment_shader = api.create_shader("shaders/gltf.frag.spv"),
        .depth           = depth_state,
    });

    depth_state.test         = VK_COMPARE_OP_GREATER_OR_EQUAL;
    depth_state.enable_write = true;

    pass.prepass = api.create_program({
        .vertex_shader   = api.create_shader("shaders/gltf.vert.spv"),
        .fragment_shader = api.create_shader("shaders/gltf_prepass.frag.spv"),
        .depth           = depth_state,
    });

    pass.shadow_cascade_program = api.create_program({
        .vertex_shader   = api.create_shader("shaders/shadowmap.vert.spv"),
        .fragment_shader = api.create_shader("shaders/shadowmap.frag.spv"),
        .depth           = depth_state,
    });

    return pass;
}

static void draw_model(vulkan::API &api, Model &model, vulkan::GraphicsProgramH program, u32 rotation_idx)
{
    // Bind the node transforms
    auto transforms_pos = api.dynamic_uniform_buffer(model.nodes.size() * sizeof(float4x4));
    auto *buffer        = reinterpret_cast<float4x4 *>(transforms_pos.mapped);
    for (uint i = 0; i < model.cached_transforms.size(); i++)
    {
        buffer[i] = model.cached_transforms[i];
    }
    api.bind_buffer(program, transforms_pos, vulkan::SHADER_DESCRIPTOR_SET, 1);

    api.bind_program(program);

    for (auto node_idx : model.nodes_preorder)
    {
        const auto &node = model.nodes[node_idx];
        const auto &mesh = model.meshes[node.mesh];

        // Draw the mesh
        for (const auto &primitive : mesh.primitives)
        {
            const auto &material = model.materials[primitive.material];

            GltfPushConstant constants = {};
            constants.node_idx         = node_idx;
            constants.vertex_offset    = primitive.first_vertex;

            constants.random_rotations_idx = rotation_idx;
            constants.base_color_idx = material.base_color_texture ? *material.base_color_texture : u32_invalid;
            constants.normal_map_idx = material.normal_texture ? *material.normal_texture : u32_invalid;
            constants.metallic_roughness_idx
                = material.metallic_roughness_texture ? *material.metallic_roughness_texture : u32_invalid;

            constants.base_color_factor = material.base_color_factor;
            constants.metallic_factor   = material.metallic_factor;
            constants.roughness_factor  = material.roughness_factor;

            api.push_constant(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                              0,
                              sizeof(constants),
                              &constants);
            api.draw_indexed(primitive.index_count, 1, primitive.first_index, 0, 0);
        }
    }
}

static void add_shadow_cascades_pass(Renderer &r, const CameraComponent &main_camera)
{
    auto &graph = r.graph;
    auto &api   = r.api;

    auto external_images = r.gltf.images;

    auto cascades_count = r.settings.shadow_cascades_count;

    auto &depth_slices  = r.depth_slices;
    auto &cascades_view = r.cascades_view;
    auto &cascades_proj = r.cascades_proj;
    depth_slices.resize(cascades_count);
    cascades_view.resize(cascades_count);
    cascades_proj.resize(cascades_count);


    // "practical split scheme" by nvidia
    // (https://developer.nvidia.com/gpugems/gpugems3/part-ii-light-and-shadows/chapter-10-parallel-split-shadow-maps-programmable-gpus)
    // basically a mix of log and uniform splits by a split factor
    float n = main_camera.far_plane;
    float f = main_camera.near_plane;
    auto N = cascades_count;
    for (uint i = 0; i < cascades_count; i++)
    {
        auto lambda = r.settings.split_factor;
        auto i_N = float(i) / (N);
        depth_slices[i] = lambda * n * std::pow(f / n, i_N) + (1 - lambda) * (n + (i_N) * (f - n));
    }

    // float far_on_range = main_camera.far_plane / (main_camera.near_plane - main_camera.far_plane);
    // float z0 = -far_on_range - 1.0f;
    // float z1 = -main_camera.near_plane * far_on_range;

    // z0 = -(f/n-f) - 1
    // z1 = - n * (f/n-f)
    // f(x) = x * (-(f/n-f) - 1) - n * (f/n-f) = 0

    for (uint i = 0; i < cascades_count; i++) {
        auto projected = main_camera.projection *  float4(0.0f, 0.0f, -depth_slices[i], 1.0f);
        depth_slices[i] = projected.z / projected.w;
    }

    // reverse the depth values
    auto copy = depth_slices;
    for (uint i = 0; i < cascades_count; i++) {
        depth_slices[i] = copy[cascades_count-1-i];
    }

    // compute view and projection  matrix for each cascade
    float last_split = 1.0f;
    for (uint i = 0; i < cascades_count; i++)
    {
        float split = depth_slices[i];

        // get the frustum of the current split (near plane = 1.0f - last_split | far plane = 1.0f - split)
        // it's a bit weird because the splits are in [0, 1] so to use reversed depth there is a 1 - depth
        std::array frustum_corners{
            float3(-1.0f, 1.0f, last_split),
            float3(1.0f, 1.0f, last_split),
            float3(1.0f, -1.0f, last_split),
            float3(-1.0f, -1.0f, last_split),
            float3(-1.0f, 1.0f, split),
            float3(1.0f, 1.0f, split),
            float3(1.0f, -1.0f, split),
            float3(-1.0f, -1.0f, split),
        };

        // project frustum corners into world space
        auto cam_inv_view_pro = main_camera.view_inverse * main_camera.projection_inverse;
        for (uint i = 0; i < 8; i++)
        {
            float4 world_space_corner = cam_inv_view_pro * float4(frustum_corners[i], 1.0f);
            frustum_corners[i]        = (1.0f / world_space_corner.w) * world_space_corner.xyz();
        }

        // get frustum center
        auto center = float3(0.0f);
        for (uint i = 0; i < 8; i++)
        {
            center = center + frustum_corners[i];
        }
        center = (1.0f / 8.0f) * center;

        // get the radius of the frustum
        float radius = 0.0f;
        for (uint i = 0; i < 8; i++)
        {
            float distance = (frustum_corners[i] - center).norm();
            radius         = std::max(radius, distance);
        }
        radius = std::ceil(radius * 16.0f) / 16.0f;

        float3 light_dir = normalize(r.sun.front);
        auto max         = float3(radius);
        float3 min       = -1.0f * max;

        cascades_view[i]
            = camera::look_at(center - light_dir * max.z,
                              center,
                              float3(0.0f, 1.0f, 0.0f)); // todo handle the case when light_dir and up are parallel

        // reverse depth
        min.z            = (max.z - min.z);
        max.z            = 0.0f;
        cascades_proj[i] = camera::ortho(min, max);

        float4x4 matrix = cascades_view[i] * cascades_proj[i];
        float4 origin   = float4(0.0f, 0.0f, 0.0f, 1.0f);
        origin          = matrix * origin;
        origin          = (2048.f / 2.0f) * origin;

        float4 rounded_origin = round(origin);
        float4 rounded_offset = rounded_origin - origin;
        rounded_offset        = (2.0f / 2048.f) * rounded_offset;
        rounded_offset.z      = 0.0f;
        rounded_offset.w      = 0.0f;

        float4x4 proj = cascades_proj[i];
        proj.at(0, 3) += rounded_offset.x;
        proj.at(1, 3) += rounded_offset.y;
        proj.at(2, 3) += rounded_offset.z;
        proj.at(3, 3) += rounded_offset.w;
        cascades_proj[i] = proj;

        last_split = split;
    }

    r.matrices_pos = api.dynamic_uniform_buffer(2 * sizeof(float4x4) * cascades_count);
    auto *matrices = reinterpret_cast<float4x4 *>(r.matrices_pos.mapped);
    for (uint i = 0; i < cascades_count; i++)
    {
        matrices[2 * i]     = cascades_view[i];
        matrices[2 * i + 1] = cascades_proj[i];
    }

    // slices are sent as-is and need to be reversed in shader
    r.depth_slices_pos = r.api.dynamic_uniform_buffer(sizeof(float4) * 2);
    auto *slices       = reinterpret_cast<float *>(r.depth_slices_pos.mapped);
    for (uint i = 0; i < cascades_count; i++)
    {
        slices[i] = r.depth_slices[i];
    }

    for (uint i = 0; i < cascades_count; i++)
    {
        auto cascade_index_pos = api.dynamic_uniform_buffer(sizeof(uint));
        auto *cascade_index    = reinterpret_cast<uint *>(cascade_index_pos.mapped);
        *cascade_index         = i;

        graph.add_pass({
            .name             = "Shadow Cascade",
            .type             = PassType::Graphics,
            .external_images  = external_images,
            .depth_attachment = r.shadow_cascades[i],
            .exec =
            [pass_data = r.gltf, cascade_index_pos, matrices_pos = r.matrices_pos, rotation_idx=r.random_rotation_idx](RenderGraph & /*graph*/,
                                                                                       RenderPass & /*self*/,
                                                                                       vulkan::API &api) {
                    // draw glTF
                    {
                        auto program = pass_data.shadow_cascade_program;

                        api.bind_buffer(program, pass_data.vertex_buffer, vulkan::SHADER_DESCRIPTOR_SET, 0);
                        api.bind_buffer(program, cascade_index_pos, vulkan::SHADER_DESCRIPTOR_SET, 2);
                        api.bind_buffer(program, matrices_pos, vulkan::SHADER_DESCRIPTOR_SET, 3);
                        api.bind_index_buffer(pass_data.index_buffer);

                        draw_model(api, *pass_data.model, program, rotation_idx);
                    }
                },
        });
    }
}

static void add_gltf_prepass(Renderer &r)
{
    auto &graph = r.graph;

    auto external_images = r.gltf.images;

    graph.add_pass({
        .name             = "glTF depth prepass",
        .type             = PassType::Graphics,
        .external_images  = external_images,
        .depth_attachment = r.depth_buffer,
        .exec =
        [pass_data = r.gltf, rotation_idx=r.random_rotation_idx](RenderGraph & /*graph*/, RenderPass & /*self*/, vulkan::API &api) {
                auto program = pass_data.prepass;

                api.bind_buffer(program, pass_data.vertex_buffer, vulkan::SHADER_DESCRIPTOR_SET, 0);
                api.bind_index_buffer(pass_data.index_buffer);

                draw_model(api, *pass_data.model, program, rotation_idx);
            },
    });
}

static void add_gltf_pass(Renderer &r)
{
    auto &graph = r.graph;

    auto external_images = r.gltf.images;
    std::vector<ImageDescH> sampled_images;
    sampled_images.push_back(r.voxels_radiance);
    for (auto volume : r.voxels_directional_volumes)
    {
        sampled_images.push_back(volume);
    }
    for (auto cascade : r.shadow_cascades)
    {
        sampled_images.push_back(cascade);
    }

    auto &depth_slices_pos = r.depth_slices_pos;

    auto matrices_pos = r.matrices_pos;

    graph.add_pass({
        .name              = "glTF pass",
        .type              = PassType::Graphics,
        .external_images   = external_images,
        .sampled_images    = sampled_images,
        .color_attachments = {r.hdr_buffer},
        .depth_attachment  = r.depth_buffer,
        .exec =
            [pass_data         = r.gltf,
             voxel_data        = r.voxels,
             trilinear_sampler = r.trilinear_sampler,
             depth_slices_pos,
             matrices_pos, rotation_idx=r.random_rotation_idx](RenderGraph &graph, RenderPass &self, vulkan::API &api) {
                auto voxels_radiance = graph.get_resolved_image(self.sampled_images[0]);
                std::vector<vulkan::ImageH> voxels_directional_volumes;
                std::vector<vulkan::ImageH> shadow_cascades;
                for (usize i = 1; i < 7; i++)
                {
                    voxels_directional_volumes.push_back(graph.get_resolved_image(self.sampled_images[i]));
                }
                for (usize i = 7; i < self.sampled_images.size(); i++)
                {
                    shadow_cascades.push_back(graph.get_resolved_image(self.sampled_images[i]));
                }

                auto program = pass_data.shading;

                api.bind_buffer(program, pass_data.vertex_buffer, vulkan::SHADER_DESCRIPTOR_SET, 0);
                // transforms will go in #1
                api.bind_buffer(program, voxel_data.vct_debug_pos, vulkan::SHADER_DESCRIPTOR_SET, 2);
                api.bind_buffer(program, voxel_data.voxel_options_pos, vulkan::SHADER_DESCRIPTOR_SET, 3);

                api.bind_combined_image_sampler(program,
                                                api.get_image(voxels_radiance).default_view,
                                                trilinear_sampler,
                                                vulkan::SHADER_DESCRIPTOR_SET,
                                                4);

                {
                    std::vector<vulkan::ImageViewH> views;
                    views.reserve(voxels_directional_volumes.size());
                    for (const auto &volume_h : voxels_directional_volumes)
                    {
                        views.push_back(api.get_image(volume_h).default_view);
                    }
                    api.bind_combined_images_samplers(program,
                                                      views,
                                                      {trilinear_sampler},
                                                      vulkan::SHADER_DESCRIPTOR_SET,
                                                      5);
                }

                api.bind_buffer(program, depth_slices_pos, vulkan::SHADER_DESCRIPTOR_SET, 6);
                api.bind_buffer(program, matrices_pos, vulkan::SHADER_DESCRIPTOR_SET, 7);

                {
                    std::vector<vulkan::ImageViewH> views;
                    views.reserve(shadow_cascades.size());
                    for (const auto &cascade_h : shadow_cascades)
                    {
                        views.push_back(api.get_image(cascade_h).default_view);
                    }
                    api.bind_combined_images_samplers(program,
                                                      views,
                                                      {trilinear_sampler},
                                                      vulkan::SHADER_DESCRIPTOR_SET,
                                                      8);
                }

                api.bind_index_buffer(pass_data.index_buffer);

                draw_model(api, *pass_data.model, program, rotation_idx);
            },
    });
}

/// --- Voxels

Renderer::VoxelPass create_voxel_pass(vulkan::API &api)
{
    Renderer::VoxelPass pass;

    pass.voxelization = api.create_program({
        .vertex_shader   = api.create_shader("shaders/voxelization.vert.spv"),
        .geom_shader     = api.create_shader("shaders/voxelization.geom.spv"),
        .fragment_shader = api.create_shader("shaders/voxelization.frag.spv"),
    });

    {
        vulkan::GraphicsProgramInfo pinfo{};
        pinfo.vertex_shader   = api.create_shader("shaders/voxel_points.vert.spv");
        pinfo.fragment_shader = api.create_shader("shaders/voxel_points.frag.spv");

        pinfo.input_assembly.topology = vulkan::PrimitiveTopology::PointList;

        pinfo.depth.enable_write = true;
        pinfo.depth.test         = VK_COMPARE_OP_GREATER_OR_EQUAL;

        pass.debug_visualization = api.create_program(std::move(pinfo));
    }

    pass.clear_voxels = api.create_program({
        .shader = api.create_shader("shaders/voxel_clear.comp.spv"),
    });

    pass.inject_radiance = api.create_program({
        .shader = api.create_shader("shaders/voxel_inject_direct_lighting.comp.spv"),
    });

    pass.generate_aniso_base = api.create_program({
        .shader = api.create_shader("shaders/voxel_gen_aniso_base.comp.spv"),
    });

    pass.generate_aniso_mipmap = api.create_program({
        .shader = api.create_shader("shaders/voxel_gen_aniso_mipmaps.comp.spv"),
    });

    return pass;
}

static void add_voxels_clear_pass(Renderer &r)
{
    auto &graph         = r.graph;
    auto &voxel_options = r.voxel_options;

    graph.add_pass({
        .name           = "Voxels clear",
        .type           = PassType::Compute,
        .storage_images = {r.voxels_albedo, r.voxels_normal, r.voxels_radiance},
        .exec =
            [pass_data = r.voxels, voxel_options](RenderGraph &graph, RenderPass &self, vulkan::API &api) {
                auto voxels_albedo   = graph.get_resolved_image(self.storage_images[0]);
                auto voxels_normal   = graph.get_resolved_image(self.storage_images[1]);
                auto voxels_radiance = graph.get_resolved_image(self.storage_images[2]);

                auto program = pass_data.clear_voxels;

                api.bind_buffer(program, pass_data.voxel_options_pos, 0);
                api.bind_image(program, api.get_image(voxels_albedo).default_view, 1);
                api.bind_image(program, api.get_image(voxels_normal).default_view, 2);
                api.bind_image(program, api.get_image(voxels_radiance).default_view, 3);

                auto count = voxel_options.res / 8;
                api.dispatch(program, count, count, count);
            },
    });
}

static void add_voxelization_pass(Renderer &r)
{
    auto &api   = r.api;
    auto &graph = r.graph;
    auto &pass_data = r.voxels;

    // Upload voxel debug
    pass_data.voxel_options_pos = api.dynamic_uniform_buffer(sizeof(VoxelOptions));
    auto *buffer0               = reinterpret_cast<VoxelOptions *>(pass_data.voxel_options_pos.mapped);
    *buffer0                    = r.voxel_options;

    // Upload projection cameras
    pass_data.projection_cameras = api.dynamic_uniform_buffer(3 * sizeof(float4x4));
    auto *buffer1                = reinterpret_cast<float4x4 *>(pass_data.projection_cameras.mapped);
    float res                    = r.voxel_options.res * r.voxel_options.size;
    float halfsize               = res / 2;
    auto center                  = r.voxel_options.center + float3(halfsize);
    float3 min_clip              = float3(-halfsize, -halfsize, 0.0f);
    float3 max_clip              = float3(halfsize, halfsize, res);
    auto projection              = camera::ortho(min_clip, max_clip);
    buffer1[0] = projection * camera::look_at(center + float3(halfsize, 0.f, 0.f), center, float3(0.f, 1.f, 0.f));
    buffer1[1] = projection * camera::look_at(center + float3(0.f, halfsize, 0.f), center, float3(0.f, 0.f, -1.f));
    buffer1[2] = projection * camera::look_at(center + float3(0.f, 0.f, halfsize), center, float3(0.f, 1.f, 0.f));

    graph.add_pass({
        .name           = "Voxelization",
        .type           = PassType::Graphics,
        .storage_images = {r.voxels_albedo, r.voxels_normal},
        .samples        = VK_SAMPLE_COUNT_32_BIT,
        .exec =
        [pass_data, model_data = r.gltf, voxel_options = r.voxel_options, rotation_idx=r.random_rotation_idx](RenderGraph &graph,
                                                                              RenderPass &self,
                                                                              vulkan::API &api) {
                auto voxels_albedo = graph.get_resolved_image(self.storage_images[0]);
                auto voxels_normal = graph.get_resolved_image(self.storage_images[1]);

                auto program = pass_data.voxelization;

                api.set_viewport_and_scissor(voxel_options.res, voxel_options.res);

                api.bind_buffer(program, model_data.vertex_buffer, vulkan::SHADER_DESCRIPTOR_SET, 0);

                api.bind_buffer(pass_data.voxelization, pass_data.voxel_options_pos, vulkan::SHADER_DESCRIPTOR_SET, 2);
                api.bind_buffer(pass_data.voxelization, pass_data.projection_cameras, vulkan::SHADER_DESCRIPTOR_SET, 3);

                auto &albedo_uint = api.get_image(voxels_albedo).format_views[0];
                auto &normal_uint = api.get_image(voxels_normal).format_views[0];
                api.bind_image(program, albedo_uint, vulkan::SHADER_DESCRIPTOR_SET, 4);
                api.bind_image(program, normal_uint, vulkan::SHADER_DESCRIPTOR_SET, 5);

                api.bind_index_buffer(model_data.index_buffer);

                draw_model(api, *model_data.model, program, rotation_idx);
            },
    });
}

static void add_voxels_debug_visualization_pass(Renderer &r)
{
    auto &graph = r.graph;
    auto &api   = r.api;

    auto options = r.voxel_options;

    auto voxels = r.voxels_albedo;
    if (r.vct_debug.display_selected == 1)
    {
        voxels = r.voxels_normal;
    }
    else if (r.vct_debug.display_selected == 2)
    {
        voxels = r.voxels_radiance;
    }
    else if (r.vct_debug.display_selected == 3)
    {
        voxels = r.voxels_directional_volumes[0];
        options.size *= 2;
    }
    else if (r.vct_debug.display_selected == 4)
    {
        voxels = r.voxels_directional_volumes[1];
        options.size *= 2;
    }
    else if (r.vct_debug.display_selected == 5)
    {
        voxels = r.voxels_directional_volumes[2];
        options.size *= 2;
    }
    else if (r.vct_debug.display_selected == 6)
    {
        voxels = r.voxels_directional_volumes[3];
        options.size *= 2;
    }
    else if (r.vct_debug.display_selected == 7)
    {
        voxels = r.voxels_directional_volumes[4];
        options.size *= 2;
    }
    else if (r.vct_debug.display_selected == 8)
    {
        voxels = r.voxels_directional_volumes[5];
        options.size *= 2;
    }

    if (r.vct_debug.voxel_selected_mip > 0)
    {
        for (int i = 0; i < r.vct_debug.voxel_selected_mip; i++)
        {
            options.size *= 2;
        }
    }

    auto options_pos = api.dynamic_uniform_buffer(sizeof(VoxelOptions));
    auto *buffer0    = reinterpret_cast<VoxelOptions *>(options_pos.mapped);
    *buffer0         = options;

    auto vertex_count = r.voxel_options.res * r.voxel_options.res * r.voxel_options.res;

    graph.add_pass({
        .name              = "Voxels debug visualization",
        .type              = PassType::Graphics,
        .sampled_images    = {voxels},
        .color_attachments = {r.hdr_buffer},
        .depth_attachment  = r.depth_buffer,
        .exec =
            [pass_data = r.voxels, sampler = r.trilinear_sampler, options_pos, vertex_count](RenderGraph &graph,
                                                                                             RenderPass &self,
                                                                                             vulkan::API &api) {
                auto voxels = graph.get_resolved_image(self.sampled_images[0]);

                auto program = pass_data.debug_visualization;

                api.bind_buffer(program, options_pos, vulkan::SHADER_DESCRIPTOR_SET, 0);
                api.bind_buffer(program, pass_data.vct_debug_pos, vulkan::SHADER_DESCRIPTOR_SET, 1);

                api.bind_combined_image_sampler(program,
                                                api.get_image(voxels).default_view,
                                                sampler,
                                                vulkan::SHADER_DESCRIPTOR_SET,
                                                2);

                api.bind_program(program);

                api.draw(vertex_count, 1, 0, 0);
            },
    });
}

static void add_voxels_direct_lighting_pass(Renderer &r)
{
    auto &graph = r.graph;

    const auto &depth_slices_pos = r.depth_slices_pos;
    const auto &matrices_pos     = r.matrices_pos;

    std::vector<ImageDescH> sampled_images = {r.voxels_albedo, r.voxels_normal};
    for (auto cascade : r.shadow_cascades)
    {
        sampled_images.push_back(cascade);
    }

    graph.add_pass({
        .name           = "Voxels direct lighting",
        .type           = PassType::Compute,
        .sampled_images = sampled_images,
        .storage_images = {r.voxels_radiance},
        .exec =
            [pass_data         = r.voxels,
             trilinear_sampler = r.trilinear_sampler,
             voxel_options     = r.voxel_options,
             depth_slices_pos,
             matrices_pos](RenderGraph &graph, RenderPass &self, vulkan::API &api) {
                auto voxels_albedo = graph.get_resolved_image(self.sampled_images[0]);
                auto voxels_normal = graph.get_resolved_image(self.sampled_images[1]);
                std::vector<vulkan::ImageH> shadow_cascades;
                shadow_cascades.reserve(self.sampled_images.size() - 2);
                for (usize i = 2; i < self.sampled_images.size(); i++)
                {
                    shadow_cascades.push_back(graph.get_resolved_image(self.sampled_images[i]));
                }

                auto voxels_radiance = graph.get_resolved_image(self.storage_images[0]);

                const auto &program = pass_data.inject_radiance;

                api.bind_buffer(program, pass_data.voxel_options_pos, 0);
                api.bind_buffer(program, pass_data.vct_debug_pos, 1);
                api.bind_combined_image_sampler(program,
                                                api.get_image(voxels_albedo).default_view,
                                                trilinear_sampler,
                                                2);
                api.bind_combined_image_sampler(program,
                                                api.get_image(voxels_normal).default_view,
                                                trilinear_sampler,
                                                3);
                api.bind_image(program, api.get_image(voxels_radiance).default_view, 4);

                api.bind_buffer(program, depth_slices_pos, 5);
                api.bind_buffer(program, matrices_pos, 6);

                {
                    std::vector<vulkan::ImageViewH> views;
                    views.reserve(shadow_cascades.size());
                    for (const auto &cascade_h : shadow_cascades)
                    {
                        views.push_back(api.get_image(cascade_h).default_view);
                    }
                    api.bind_combined_images_samplers(program, views, {trilinear_sampler}, 7);
                }

                auto count = voxel_options.res / 8;
                api.dispatch(program, count, count, count);
            },
    });
}

static void add_voxels_aniso_filtering(Renderer &r)
{
    auto &api   = r.api;
    auto &graph = r.graph;

    api.begin_label("Compute anisotropic voxels");

    auto voxel_size = r.voxel_options.size * 2;
    auto voxel_res  = r.voxel_options.res / 2;

    auto mip_pos = api.dynamic_uniform_buffer(sizeof(VoxelOptions));
    {
        auto *buffer = reinterpret_cast<VoxelOptions *>(mip_pos.mapped);
        *buffer      = r.voxel_options;
        buffer->size = voxel_size;
        buffer->res  = voxel_res;
    }

    std::vector<ImageDescH> storage_images;
    for (auto image : r.voxels_directional_volumes)
    {
        storage_images.push_back(image);
    }

    auto count = r.voxel_options.res / 8; // local compute size

    auto &trilinear_sampler = r.trilinear_sampler;

    graph.add_pass({
        .name           = "Voxels aniso base",
        .type           = PassType::Compute,
        .sampled_images = {r.voxels_radiance},
        .storage_images = storage_images,
        .exec =
            [pass_data = r.voxels, trilinear_sampler, count, mip_pos](RenderGraph &graph,
                                                                      RenderPass &self,
                                                                      vulkan::API &api) {
                // resolved directional volumes
                std::vector<vulkan::ImageH> voxels_directional_volumes;
                std::vector<vulkan::ImageViewH> views;
                views.reserve(self.storage_images.size());
                for (auto volume : self.storage_images)
                {
                    auto volume_h = graph.get_resolved_image(volume);
                    voxels_directional_volumes.push_back(volume_h);

                    auto &image = api.get_image(volume_h);
                    views.push_back(image.mip_views[0]);
                }

                auto voxels_radiance = graph.get_resolved_image(self.sampled_images[0]);

                auto program = pass_data.generate_aniso_base;

                api.bind_buffer(program, mip_pos, 0);
                api.bind_combined_image_sampler(program,
                                                api.get_image(voxels_radiance).default_view,
                                                trilinear_sampler,
                                                1);
                api.bind_images(program, views, 2);

                api.dispatch(program, count, count, count);
            },
    });

    for (uint mip_i = 0; count > 1; mip_i++)
    {
        count /= 2;
        voxel_size *= 2;
        voxel_res /= 2;

        auto src = mip_i;
        auto dst = mip_i + 1;

        // Bind voxel options
        mip_pos = api.dynamic_uniform_buffer(sizeof(VoxelOptions));
        {
            auto *buffer = reinterpret_cast<VoxelOptions *>(mip_pos.mapped);
            *buffer      = r.voxel_options;
            buffer->size = voxel_size;
            buffer->res  = voxel_res;
        }

        auto mip_src_pos = api.dynamic_uniform_buffer(sizeof(int));
        {
            auto *buffer = reinterpret_cast<int *>(mip_src_pos.mapped);
            *buffer      = static_cast<int>(src);
        }

        graph.add_pass({
            .name           = "Voxels aniso mip level",
            .type           = PassType::Compute,
            .sampled_images = {r.voxels_radiance},
            .storage_images = storage_images,
            .exec =
                [pass_data = r.voxels, count, mip_pos, mip_src_pos, src, dst](RenderGraph &graph,
                                                                              RenderPass &self,
                                                                              vulkan::API &api) {
                    // resolved directional volumes
                    std::vector<vulkan::ImageH> voxels_directional_volumes;
                    voxels_directional_volumes.reserve(self.storage_images.size());
                    for (auto volume : self.storage_images)
                    {
                        auto volume_h = graph.get_resolved_image(volume);
                        voxels_directional_volumes.push_back(volume_h);
                    }

                    auto program = pass_data.generate_aniso_mipmap;
                    api.bind_buffer(program, mip_pos, 0);
                    api.bind_buffer(program, mip_src_pos, 1);

                    std::vector<vulkan::ImageViewH> src_views;
                    std::vector<vulkan::ImageViewH> dst_views;
                    src_views.reserve(voxels_directional_volumes.size());
                    dst_views.reserve(voxels_directional_volumes.size());

                    for (const auto &volume_h : voxels_directional_volumes)
                    {
                        auto &image = api.get_image(volume_h);
                        src_views.push_back(image.mip_views[src]);
                        dst_views.push_back(image.mip_views[dst]);
                    }

                    api.bind_images(program, src_views, 2);
                    api.bind_images(program, dst_views, 3);

                    api.dispatch(program, count, count, count);
                },
        });
    }

    api.end_label();
}

/// ---

void update_uniforms(Renderer &r, ECS::World &world, ECS::EntityId main_camera)
{
    const auto &camera_transform = *world.get_component<TransformComponent>(main_camera);
    const auto &input_camera = *world.get_component<InputCameraComponent>(main_camera);
    auto &camera = *world.get_component<CameraComponent>(main_camera);

    float aspect_ratio = r.settings.render_resolution.x / float(r.settings.render_resolution.y);

    camera.view = camera::look_at(camera_transform.position, input_camera.target, float3_UP, &camera.view_inverse);
    camera.projection  = camera::perspective(camera.fov,
                                            aspect_ratio,
                                            camera.near_plane,
                                            camera.far_plane,
                                            &camera.projection_inverse);

    auto &api = r.api;
    api.begin_label("Update uniforms");

    r.global_uniform_pos = api.dynamic_uniform_buffer(sizeof(GlobalUniform));
    auto *globals        = reinterpret_cast<GlobalUniform *>(r.global_uniform_pos.mapped);
    std::memset(globals, 0, sizeof(GlobalUniform));

    globals->delta_t              = r.p_timer->get_delta_time();
    globals->camera_pos           = camera_transform.position;
    globals->camera_view          = camera.view;
    globals->camera_proj          = camera.projection;
    globals->camera_inv_proj      = camera.projection_inverse;
    globals->camera_inv_view_proj = camera.view_inverse * camera.projection_inverse;
    globals->camera_near          = camera.near_plane;
    globals->camera_far           = camera.far_plane;
    globals->sun_view             = float4x4(0.0f);
    globals->sun_proj             = float4x4(0.0f);

    auto resolution_scale  = r.settings.resolution_scale;
    globals->resolution    = {
        static_cast<uint>(resolution_scale * r.settings.render_resolution.x),
        static_cast<uint>(resolution_scale * r.settings.render_resolution.y)
    };

    {
        auto pitch = to_radians(r.sun.pitch);
        auto yaw = to_radians(-r.sun.yaw);
        auto roll = to_radians(r.sun.roll);

        auto y_m     = float4x4::identity();
        y_m.at(0, 0) = cos(yaw);
        y_m.at(0, 1) = -sin(yaw);
        y_m.at(1, 0) = sin(yaw);
        y_m.at(1, 1) = cos(yaw);

        auto p_m     = float4x4::identity();
        p_m.at(0, 0) = cos(pitch);
        p_m.at(0, 2) = sin(pitch);
        p_m.at(2, 0) = -sin(pitch);
        p_m.at(2, 2) = cos(pitch);

        auto r_m     = float4x4::identity();
        r_m.at(1, 1) = cos(roll);
        r_m.at(1, 2) = -sin(roll);
        r_m.at(2, 1) = sin(roll);
        r_m.at(2, 2) = cos(roll);

        auto R = y_m * p_m * r_m;

        r.sun.front = (R * float4(float3(0.0f, 0.0f, 1.0f), 1.0f)).xyz();
    }
    globals->sun_direction = -1.0f * r.sun.front;
    // TODO: move from global and use real values (will need auto exposure)
    globals->sun_illuminance = r.sun_illuminance;
    globals->ambient         = r.ambient;

    r.api.bind_buffer({}, r.global_uniform_pos, vulkan::GLOBAL_DESCRIPTOR_SET, 0);

    std::vector<vulkan::ImageViewH> views;
    std::vector<vulkan::SamplerH> samplers;

    for (uint i = 0; i < r.gltf.model->textures.size(); i++)
    {
        const auto &texture = r.gltf.model->textures[i];
        auto image_h        = r.gltf.images[texture.image];
        auto &image         = api.get_image(image_h);
        views.push_back(image.default_view);
        samplers.push_back(r.gltf.samplers[texture.sampler]);
    }

    r.random_rotation_idx = views.size();
    views.push_back(r.api.get_image(r.random_rotations).default_view);
    samplers.push_back(r.nearest_sampler);

    api.bind_combined_images_samplers({}, views, samplers, vulkan::GLOBAL_DESCRIPTOR_SET, 1);

    r.api.update_global_set();

    r.voxels.vct_debug_pos = api.dynamic_uniform_buffer(sizeof(VCTDebug));
    auto *debug            = reinterpret_cast<VCTDebug *>(r.voxels.vct_debug_pos.mapped);
    *debug                 = r.vct_debug;

    r.voxels.voxel_options_pos = api.dynamic_uniform_buffer(sizeof(VoxelOptions));
    auto *buffer0              = reinterpret_cast<VoxelOptions *>(r.voxels.voxel_options_pos.mapped);
    *buffer0                   = r.voxel_options;

    api.end_label();
}

/// --- Where the magic happens

void Renderer::display_ui(UI::Context &ui)
{
    graph.display_ui(ui);

    ImGuiIO &io = ImGui::GetIO();
    auto &timer = *p_timer;

    io.DeltaTime = timer.get_delta_time();
    io.Framerate = timer.get_average_fps();

    if (ui.begin_window("Profiler", true))
    {
        static bool show_fps = false;

        if (ImGui::RadioButton("FPS", show_fps))
        {
            show_fps = true;
        }

        ImGui::SameLine();

        if (ImGui::RadioButton("ms", !show_fps))
        {
            show_fps = false;
        }

        if (show_fps)
        {
            ImGui::SetCursorPosX(20.0f);
            ImGui::Text("%7.1f", double(timer.get_average_fps()));

            const auto &histogram = timer.get_fps_histogram();
            ImGui::PlotHistogram("",
                                 histogram.data(),
                                 static_cast<int>(histogram.size()),
                                 0,
                                 nullptr,
                                 0.0f,
                                 FLT_MAX,
                                 ImVec2(85.0f, 30.0f));
        }
        else
        {
            ImGui::SetCursorPosX(20.0f);
            ImGui::Text("%9.3f", double(timer.get_average_delta_time()));

            const auto &histogram = timer.get_delta_time_histogram();
            ImGui::PlotHistogram("",
                                 histogram.data(),
                                 static_cast<int>(histogram.size()),
                                 0,
                                 nullptr,
                                 0.0f,
                                 FLT_MAX,
                                 ImVec2(85.0f, 30.0f));
        }

        const auto &timestamps = api.timestamps;
        if (!timestamps.empty())
        {
            ImGui::Columns(3, "timestamps"); // 4-ways, with border
            ImGui::Separator();
            ImGui::Text("Label");
            ImGui::NextColumn();
            ImGui::Text("GPU (us)");
            ImGui::NextColumn();
            ImGui::Text("CPU (ms)");
            ImGui::NextColumn();
            ImGui::Separator();
            for (uint32_t i = 1; i < timestamps.size() - 1; i++)
            {
                auto gpu_delta = timestamps[i].gpu_microseconds - timestamps[i - 1].gpu_microseconds;
                auto cpu_delta = timestamps[i].cpu_milliseconds - timestamps[i - 1].cpu_milliseconds;

                ImGui::Text("%*s", static_cast<int>(timestamps[i].label.size()), timestamps[i].label.data());
                ImGui::NextColumn();
                ImGui::Text("%.1f", gpu_delta);
                ImGui::NextColumn();
                ImGui::Text("%.1f", cpu_delta);
                ImGui::NextColumn();
            }

            ImGui::Columns(1);
            ImGui::Separator();

            // scrolling data and average computing
            static std::array<float, 128> gpu_values;
            static std::array<float, 128> cpu_values;

            gpu_values[127]   = float(timestamps.back().gpu_microseconds - timestamps.front().gpu_microseconds);
            cpu_values[127]   = float(timestamps.back().cpu_milliseconds - timestamps.front().cpu_milliseconds);
            float gpu_average = gpu_values[0];
            float cpu_average = cpu_values[0];
            for (uint i = 0; i < 128 - 1; i++)
            {
                gpu_values[i] = gpu_values[i + 1];
                gpu_average += gpu_values[i];
                cpu_values[i] = cpu_values[i + 1];
                cpu_average += cpu_values[i];
            }
            gpu_average /= 128;
            cpu_average /= 128;

            ImGui::Text("%-17s: %7.1f us", "Total GPU time", gpu_average);
            ImGui::PlotLines("", gpu_values.data(), 128, 0, "", 0.0f, 30000.0f, ImVec2(0, 80));

            ImGui::Text("%-17s: %7.1f ms", "Total CPU time", cpu_average);
            ImGui::PlotLines("", cpu_values.data(), 128, 0, "", 0.0f, 30000.0f, ImVec2(0, 80));
        }

        ui.end_window();
    }

    if (ui.begin_window("Renderer", true))
    {
        if (ImGui::CollapsingHeader("API"))
        {
            api.display_ui(ui);
        }
        if (ImGui::CollapsingHeader("Settings"))
        {
            auto old_scale = settings.resolution_scale;
            ImGui::SliderFloat("Resolution scale", &settings.resolution_scale, 0.25f, 1.0f);
            if (settings.resolution_scale != old_scale)
            {
                settings.resolution_dirty = true;
            }

            int cascades_count = settings.shadow_cascades_count;
            ImGui::InputInt("Cascades count", &cascades_count, 1, 2);
            (void)(cascades_count);

            ImGui::Text("Render resolution: %ux%u", settings.render_resolution.x, settings.render_resolution.y);
            ImGui::SliderFloat("Split factor", &settings.split_factor, 0.1f, 1.0f);
            ImGui::Checkbox("Display grid", &settings.show_grid);
        }

        if (ImGui::CollapsingHeader("Global"))
        {
            ImGui::SliderFloat("Sun Illuminance", &sun_illuminance.x, 1.0f, 100.0f);

            sun_illuminance.y = sun_illuminance.x;
            sun_illuminance.z = sun_illuminance.x;

            ImGui::SliderFloat3("Sun rotation", &sun.pitch, -180.0f, 180.0f);
            ImGui::SliderFloat("Ambient", &ambient, 0.0f, 1.0f);
        }

        if (ImGui::CollapsingHeader("Cascaded Shadow maps"))
        {
            ImGui::TextUnformatted("Depth slices:");
            for (auto depth_slice : depth_slices)
            {
                ImGui::Text("  %f", depth_slice);
            }
            for (auto shadow_map : shadow_cascades)
            {
                auto &res = graph.images.at(shadow_map);
                if (res.resolved_img.is_valid())
                {
                    ImGui::Image((void *)(api.get_image(res.resolved_img).default_view.hash()), ImVec2(512, 512));
                }
            }
        }

        ui.end_window();
    }

    if (ui.begin_window("Shaders", true))
    {
        if (ImGui::CollapsingHeader("Voxel Cone Tracing"))
        {
            ImGui::TextUnformatted("Debug display: ");
            ImGui::SameLine();
            if (ImGui::RadioButton("glTF", vct_debug.display == 0))
            {
                vct_debug.display = 0;
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("voxels", vct_debug.display == 1))
            {
                vct_debug.display = 1;
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("custom", vct_debug.display == 2))
            {
                vct_debug.display = 2;
            }

            if (vct_debug.display == 0)
            {
                static std::array options{"Nothing",
                                          "BaseColor",
                                          "Normals",
                                          "AO",
                                          "Indirect lighting",
                                          "Direct lighting"};
                tools::imgui_select("Debug output", options.data(), options.size(), vct_debug.display_selected);
                ImGui::SliderFloat("Trace dist.", &vct_debug.trace_dist, 0.0f, 30.0f);
                ImGui::SliderFloat("Occlusion factor", &vct_debug.occlusion_lambda, 0.0f, 1.0f);
                ImGui::SliderFloat("Sampling factor", &vct_debug.sampling_factor, 0.1f, 2.0f);
                ImGui::SliderFloat("Start position (in voxel)", &vct_debug.start, 0.0f, 2.0f);
            }
            else
            {
                static std::array options{
                    "Albedo",
                    "Normal",
                    "Radiance",
                    "Voxels volume -X",
                    "Voxels volume +X",
                    "Voxels volume -Y",
                    "Voxels volume +Y",
                    "Voxels volume -Z",
                    "Voxels volume +Z",
                };
                tools::imgui_select("Debug output", options.data(), options.size(), vct_debug.display_selected);

                ImGui::SliderInt("Mip level", &vct_debug.voxel_selected_mip, 0, 10);
            }
        }

        if (ImGui::CollapsingHeader("Voxelization"))
        {
            ImGui::SliderFloat3("Center", &voxel_options.center.raw[0], -40.f, 40.f);
            ImGui::SliderFloat("Voxel size (m)", &voxel_options.size, 0.05f, 0.5f);
        }

        if (ImGui::CollapsingHeader("Tonemapping"))
        {
            static std::array options{"Reinhard", "Exposure", "Clamp", "ACES"};
            tools::imgui_select("Tonemap", options.data(), options.size(), tonemap_debug.selected);
            ImGui::SliderFloat("Exposure", &tonemap_debug.exposure, 0.0f, 2.0f);
        }

        ui.end_window();
    }

    ImGuiWindowFlags fb_flags = ImGuiWindowFlags_NoDecoration;
    if (ui.begin_window("Framebuffer", true, fb_flags))
    {
        float2 max = ImGui::GetWindowContentRegionMax();
        float2 min = ImGui::GetWindowContentRegionMin();
        float2 size = float2(min.x < max.x ? max.x - min.x : min.x, min.y < max.y ? max.y - min.y : min.y);

        if (static_cast<uint>(size.x) != settings.render_resolution.x || static_cast<uint>(size.y) != settings.render_resolution.y)
        {
            settings.render_resolution.x = static_cast<uint>(size.x);
            settings.render_resolution.y = static_cast<uint>(size.y);
            settings.resolution_dirty = true;
        }

        auto image = graph.get_resolved_image(ldr_buffer);
        if (image.is_valid())
        {
            ImGui::Image((void *)(api.get_image(image).default_view.hash()), size);
        }

        ui.end_window();
    }
}

void Renderer::draw(ECS::World &world, ECS::EntityId main_camera)
{

    if (settings.resolution_dirty)
    {
        wait_idle();
        graph.on_resize(settings.render_resolution.x * settings.resolution_scale, settings.render_resolution.y * settings.resolution_scale);
        settings.resolution_dirty = false;
    }

    bool is_ok = api.start_frame();
    if (!is_ok)
    {
        ImGui::EndFrame();
        return;
    }
    graph.clear(); // start_frame() ?

    update_uniforms(*this, world, main_camera);

    add_shadow_cascades_pass(*this, *world.get_component<CameraComponent>(main_camera));

    // voxel cone tracing prep
    add_voxels_clear_pass(*this);
    add_voxelization_pass(*this);
    add_voxels_direct_lighting_pass(*this);
    add_voxels_aniso_filtering(*this);

    if (vct_debug.display != 0)
    {
        add_voxels_debug_visualization_pass(*this);
    }
    else
    {
        add_gltf_prepass(*this);
        add_gltf_pass(*this);
    }

    auto *sky_atmosphere = world.singleton_get_component<SkyAtmosphereComponent>();
    if (sky_atmosphere)
    {
        add_procedural_sky_pass(*this, *sky_atmosphere);
    }

    add_luminance_pass(*this);

    add_tonemapping_pass(*this);

    if (settings.show_grid)
    {
        add_floor_pass(*this);
    }

    // graph.add_pass({.name = "Blit to swapchain", .type = PassType::BlitToSwapchain, .color_attachments = {ldr_buffer}});

    add_imgui_pass(*this);

    ImGui::EndFrame(); // right before drawing the ui

    if (!graph.execute())
    {
        return;
    }

    api.end_frame();
}

} // namespace my_app
