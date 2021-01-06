#include "render/renderer.hpp"

#include "camera.hpp"
#include "components/camera_component.hpp"
#include "components/input_camera_component.hpp"
#include "components/sky_atmosphere_component.hpp"
#include "components/transform_component.hpp"
#include "gltf.hpp"
#include "render/hl_api.hpp"
#include "render/luminance_pass.hpp"
#include "render/render_graph.hpp"
#include "timer.hpp"
#include "tools.hpp"
#include "ui.hpp"

#include <algorithm>
#include <array>
#include <cstring> // for memset
#include <execution>
#include <fmt/core.h>
#include <future>
#include <imgui/imgui.h>
#include <random>
#include <ranges>
#include <vulkan/vulkan_core.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>


struct DrawData
{
    u32 transform_idx;
    u32 vertex_idx;
    u32 material_idx;
    u32 primitive_idx;
};

namespace my_app
{

Renderer::TemporalPass create_temporal_pass(vulkan::API &api)
{
    Renderer::TemporalPass pass;
    pass.accumulate = api.create_program({
        .shader   = api.create_shader("shaders/temporal_accumulation.comp.glsl.spv")
    });
    return pass;
}

// frame data
void Renderer::create(Renderer &r, const platform::Window &window, TimerData &timer)
{
    // where to put this code?
    //

    vulkan::API::create(r.api, window);
    RenderGraph::create(r.graph, r.api);

    std::string path = fmt::format("../models/{0}/glTF/{0}.gltf", "Sponza");
    path = fmt::format("../models/{}/scene.gltf", "san-miguel"); // "chocobo-blender" "medieval_fantasy_book" "huge_medieval_battle_scene" "san-miguel"
    r.model = std::make_shared<Model>(load_model(path)); // TODO: where??

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
        .count  = 512,
    });

    r.api.create_global_set();

    create_imgui_pass(r.imgui, r.graph, r.api);

    r.checkerboard_floor = create_floor_pass(r.api);
    r.procedural_sky     = create_procedural_sky_pass(r.api);
    r.tonemapping        = create_tonemapping_pass(r.api);
    r.gltf               = create_gltf_pass(r.graph, r.api, r.model);
    r.voxels             = create_voxel_pass(r.api);
    r.luminance          = create_luminance_pass(r.api);
    r.cascades_bounds    = create_cascades_bounds_pass(r.api);
    r.temporal_pass      = create_temporal_pass(r.api);

    // basic resources

    r.settings            = {};
    r.settings.render_resolution = {.x = r.api.ctx.swapchain.extent.width, .y = r.api.ctx.swapchain.extent.height};

    r.graph.on_resize(r.settings.render_resolution.x * r.settings.resolution_scale, r.settings.render_resolution.y * r.settings.resolution_scale);

    r.trilinear_sampler = r.api.trilinear_sampler;
    r.nearest_sampler = r.api.nearest_sampler;

    r.nearest_repeat_sampler = r.api.create_sampler({
        .mag_filter   = VK_FILTER_NEAREST,
        .min_filter   = VK_FILTER_NEAREST,
        .mip_map_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .address_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        });
    r.trilinear_repeat_sampler = r.api.create_sampler({
        .mag_filter   = VK_FILTER_LINEAR,
        .min_filter   = VK_FILTER_LINEAR,
        .mip_map_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
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

    vulkan::ImageH *random_rotations_h = &r.random_rotations;

    r.graph.add_pass({
        .name             = "Upload random rotations",
        .type             = PassType::Compute,
        .exec =
            [=](RenderGraph & /*graph*/, RenderPass & /*self*/, vulkan::API &api) {

                api.upload_image(*random_rotations_h, rotations.data(), sizeof(rotations));
            },
    });

    r.sun.pitch    = 0.0f;
    r.sun.yaw      = 25.0f;
    r.sun.roll     = 80.0f;

    auto compute_halton = [](int index, int radix)
        {
            float result = 0.f;
            float fraction = 1.f / float(radix);

            while (index > 0)
            {
                result += float(index % radix) * fraction;

                index /= radix;
                fraction /= float(radix);
            }

            return result;
        };

    for (usize i_halton = 0; i_halton < 16; i_halton++)
    {
        r.halton_indices[i_halton].x = compute_halton(i_halton + 1, 2);
        r.halton_indices[i_halton].y = compute_halton(i_halton + 1, 3);
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

void Renderer::reload_shader(std::string_view shader_name)
{
    fmt::print(stderr, "{} changed!\n", shader_name);

    // Find the shader that needs to be updated
    vulkan::Shader *found = nullptr;
    for (auto &[shader_h, shader] : api.shaders) {
        fmt::print(stderr, "{} == {}\n", shader_name, shader->name);

        if (shader_name == shader->name) {
            assert(found == nullptr);
            found = &(*shader);
        }
    }

    if (!found) {
        assert(false);
        return;
    }

    vulkan::Shader &shader = *found;
    fmt::print(stderr, "Found {}\n", shader.name);

    // Create a new shader module
    vulkan::ShaderH new_shader = api.create_shader(shader_name);
    fmt::print(stderr, "New shader's handle:  {}\n", new_shader.value());

    std::vector<vulkan::ShaderH> to_remove;

    // Update programs using this shader to the new shader
    for (auto &[program_h, program] : api.graphics_programs) {
        if (program->info.vertex_shader.is_valid()) {
            auto &vertex_shader = api.get_shader(program->info.vertex_shader);
            if (vertex_shader.name == shader.name) {
                to_remove.push_back(program->info.vertex_shader);
                program->info.vertex_shader = new_shader;
            }
        }

        if (program->info.geom_shader.is_valid()) {
            auto &geom_shader = api.get_shader(program->info.geom_shader);
            if (geom_shader.name == shader.name) {
                to_remove.push_back(program->info.geom_shader);
                program->info.geom_shader = new_shader;
            }
        }

        if (program->info.fragment_shader.is_valid()) {
            auto &fragment_shader = api.get_shader(program->info.fragment_shader);
            if (fragment_shader.name == shader.name) {
                to_remove.push_back(program->info.fragment_shader);
                program->info.fragment_shader = new_shader;
            }
        }
    }

    for (auto &[program_h, program] : api.compute_programs) {
        if (program->info.shader.is_valid()) {
            auto &compute_shader = api.get_shader(program->info.shader);
            if (compute_shader.name == shader.name) {
                to_remove.push_back(program->info.shader);
                program->info.shader = new_shader;
            }
        }
    }

    assert(!to_remove.empty());

    // Destroy the old shaders
    for (vulkan::ShaderH shader_h : to_remove) {
        fmt::print(stderr, "Removing handle:  {}\n", new_shader.value());
        api.destroy_shader(shader_h);
    }
}

/// --- glTF model pass

Renderer::GltfPass create_gltf_pass(RenderGraph &graph, vulkan::API &api, std::shared_ptr<Model> &_model)
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

    std::vector<u32> parent_indices;
    parent_indices.reserve(model.nodes.size());

    for (auto scene_root : model.scene)
    {
        nodes_stack.push_back(u32_invalid);
        nodes_stack.push_back(scene_root);
    }

    while (!nodes_stack.empty())
    {
        auto node_idx = nodes_stack.back();
        nodes_stack.pop_back();

        if (node_idx == u32_invalid)
        {
            if (!parent_indices.empty()) {
                parent_indices.pop_back();
            }
            continue;
        }

        auto &node = model.nodes[node_idx];

        // --- preorder
        float constant_scale = 1.0f;

        node.dirty                        = false;
        auto translation                  = float4x4::identity(); // glm::translate(glm::mat4(1.0f), node.translation);
        translation = float4x4({
                1, 0, 0, constant_scale * node.translation.x,
                0, 1, 0, constant_scale * node.translation.y,
                0, 0, 1, constant_scale * node.translation.z,
                0, 0, 0, 1,
            });

        auto rotation                     = float4x4::identity(); // glm::mat4(node.rotation);
        rotation = float4x4({
                1.0f - 2.0f*node.rotation.y*node.rotation.y - 2.0f*node.rotation.z*node.rotation.z, 2.0f*node.rotation.x*node.rotation.y - 2.0f*node.rotation.z*node.rotation.w, 2.0f*node.rotation.x*node.rotation.z + 2.0f*node.rotation.y*node.rotation.w, 0.0f,
                2.0f*node.rotation.x*node.rotation.y + 2.0f*node.rotation.z*node.rotation.w, 1.0f - 2.0f*node.rotation.x*node.rotation.x - 2.0f*node.rotation.z*node.rotation.z, 2.0f*node.rotation.y*node.rotation.z - 2.0f*node.rotation.x*node.rotation.w, 0.0f,
                2.0f*node.rotation.x*node.rotation.z - 2.0f*node.rotation.y*node.rotation.w, 2.0f*node.rotation.y*node.rotation.z + 2.0f*node.rotation.x*node.rotation.w, 1.0f - 2.0f*node.rotation.x*node.rotation.x - 2.0f*node.rotation.y*node.rotation.y, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f,
            });

        auto scale                        = float4x4::identity(); // assume uniform scale
        scale.at(0, 0)                    = constant_scale * node.scale.x;
        scale.at(1, 1)                    = constant_scale * node.scale.y;
        scale.at(2, 2)                    = constant_scale * node.scale.z;

        auto transform = node.transform * translation * rotation * scale;

        auto parent_transform = float4x4::identity();
        if (!parent_indices.empty()) {
            parent_transform = model.cached_transforms[parent_indices.back()];
        }

        model.cached_transforms[node_idx] = parent_transform * transform;

        model.nodes_preorder.push_back(node_idx);

        parent_indices.push_back(node_idx);

        nodes_stack.push_back(u32_invalid);
        // ----

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
        .usage = vulkan::storage_buffer_usage,
    });

    usize ibuffer_size = model.indices.size() * sizeof(u32);
    pass.index_buffer  = api.create_buffer({
        .name  = "glTF Index Buffer",
        .size  = ibuffer_size,
        .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    });

    usize matbuffer_size = model.materials.size() * sizeof(Material);
    pass.material_buffer = api.create_buffer({
        .name  = "Materials",
        .size  = matbuffer_size,
        .usage = vulkan::storage_buffer_usage,
    });

    usize primbuffer_size = model.primitives.size() * sizeof(Primitive);
    pass.primitives_buffer = api.create_buffer({
        .name  = "Primitives",
        .size  = primbuffer_size,
        .usage = vulkan::storage_buffer_usage,
    });

    usize transformsbuffer_size = model.cached_transforms.size() * sizeof(float4x4);
    pass.transforms_buffer = api.create_buffer({
        .name  = "Transforms",
        .size  = transformsbuffer_size,
        .usage = vulkan::storage_buffer_usage,
    });


    // Upload draw data
    u32 draw_count = 0;
    for (auto node_idx : model.nodes_preorder)
    {
        const auto &node = model.nodes[node_idx];
        if (!node.mesh) {
            continue;
        }

        const auto &mesh = model.meshes[*node.mesh];
        draw_count += mesh.primitives.size();
    }

    usize drawbuffer_size = draw_count * sizeof(DrawData);
    std::vector<DrawData> draws;
    draws.reserve(draw_count);
    for (auto node_idx : model.nodes_preorder)
    {
        const auto &node = model.nodes[node_idx];
        if (!node.mesh) {
            continue;
        }
        const auto &mesh = model.meshes[*node.mesh];
        for (auto primitive_idx : mesh.primitives)
        {
            const auto &primitive = model.primitives[primitive_idx];
            DrawData draw_data;
            draw_data.transform_idx = node_idx;
            draw_data.vertex_idx = primitive.first_vertex;
            draw_data.material_idx = primitive.material;
            draw_data.primitive_idx = primitive_idx;
            draws.push_back(draw_data);
        }
    }
    pass.draws_buffer = api.create_buffer({
        .name  = "Draws data",
        .size  = drawbuffer_size,
        .usage = vulkan::storage_buffer_usage,
    });

    pass.commands = {};
    for (auto node_idx : model.nodes_preorder)
    {
        const auto &node = model.nodes[node_idx];
        if (!node.mesh) {
            continue;
        }

        const auto &mesh = model.meshes[*node.mesh];

        // Draw the mesh
        for (auto primitive_idx : mesh.primitives)
        {
            const auto &primitive = model.primitives[primitive_idx];
            pass.commands.draw_count += 1;
            pass.commands.commands.push_back({
                    .index_count = primitive.index_count,
                    .instance_count = 1,
                    .first_index = primitive.first_index,
                    .vertex_offset = 0,
                    .first_instance = 0,
                });
        }
    }
    usize commandsbuffer_size = sizeof(u32) + pass.commands.commands.size() * sizeof(vulkan::DrawIndirectCommand);
    pass.commands_buffer = api.create_buffer({
        .name  = "Draw Indirect commands",
        .size  = commandsbuffer_size,
        .usage = vulkan::storage_buffer_usage | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
    });

    auto *p_model = _model.get();
    graph.add_pass({
        .name = "Upload glTF buffers",
        .type = PassType::Compute,
        .exec =
            [=](RenderGraph & /*graph*/, RenderPass & /*self*/, vulkan::API &api) {

                api.upload_buffer(pass.vertex_buffer, p_model->vertices.data(), vbuffer_size);
                api.upload_buffer(pass.index_buffer, p_model->indices.data(), ibuffer_size);
                api.upload_buffer(pass.material_buffer, p_model->materials.data(), matbuffer_size);
                api.upload_buffer(pass.primitives_buffer, p_model->primitives.data(), primbuffer_size);
                api.upload_buffer(pass.transforms_buffer, p_model->cached_transforms.data(), transformsbuffer_size);
                api.upload_buffer(pass.draws_buffer, draws.data(), drawbuffer_size);
                api.upload_buffer(pass.commands_buffer, &pass.commands.draw_count, sizeof(u32));
                api.upload_buffer(pass.commands_buffer,
                                  pass.commands.commands.data(),
                                  pass.commands.commands.size() * sizeof(vulkan::DrawIndirectCommand),
                                  sizeof(u32));

            },
    });

    pass.visibility_buffer = api.create_buffer({
        .name  = "Draw visibility",
        .size  = sizeof(u32) * 2048,
        .usage = vulkan::storage_buffer_usage,
    });

    pass.finalcommands_buffer = api.create_buffer({
        .name  = "Culled draw indirect commands",
        .size  = commandsbuffer_size,
        .usage = vulkan::storage_buffer_usage | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
    });

    pass.finaldata_buffer = api.create_buffer({
        .name  = "Culled draw data",
        .size  = drawbuffer_size,
        .usage = vulkan::storage_buffer_usage,
    });

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

    struct StbImage
    {
        int width       = 0;
        int height      = 0;
        u8 *pixels      = nullptr;
        int nb_comp     = 0;
        VkFormat format = VK_FORMAT_UNDEFINED;
    };

    std::vector<StbImage> stb_images;
    stb_images.resize(model.images.size());

    std::vector<uint> indices(model.images.size());
    std::iota(indices.begin(), indices.end(), 0);

    parallel_foreach(indices, [&](uint i) {
        const auto &image = model.images[i];
        auto &stb_image   = stb_images[i];

        stb_image.pixels = stbi_load_from_memory(image.data.data(),
                                                 static_cast<int>(image.data.size()),
                                                 &stb_image.width,
                                                 &stb_image.height,
                                                 &stb_image.nb_comp,
                                                 0);

        if (stb_image.nb_comp == 1) // NOLINT
        {
            stb_image.format = VK_FORMAT_R8_UNORM;
        }
        else if (stb_image.nb_comp == 2) // NOLINT
        {
            stb_image.format = VK_FORMAT_R8G8_UNORM;
        }
        else if (stb_image.nb_comp == 3) // NOLINT
        {
            stbi_image_free(stb_image.pixels);
            int wanted_nb_comp = 4;
            stb_image.pixels   = stbi_load_from_memory(image.data.data(),
                                                     static_cast<int>(image.data.size()),
                                                     &stb_image.width,
                                                     &stb_image.height,
                                                     &stb_image.nb_comp,
                                                     wanted_nb_comp);
            stb_image.format   = image.srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
            stb_image.nb_comp  = wanted_nb_comp;
        }
        else if (stb_image.nb_comp == 4) // NOLINT
        {
            stb_image.format = image.srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
        }
        else // NOLINT
        {
            assert(false);
        }
    });

    assert(stb_images.size() == model.images.size());

    for (auto &stb_image : stb_images)
    {
        vulkan::ImageInfo iinfo;
        iinfo.name                = "glTF image";
        iinfo.width               = static_cast<u32>(stb_image.width);
        iinfo.height              = static_cast<u32>(stb_image.height);
        iinfo.depth               = 1;
        iinfo.format              = stb_image.format;
        iinfo.generate_mip_levels = true;

        auto image_h = api.create_image(iinfo);
        pass.images.push_back(image_h);

        graph.add_pass({
            .name = "Upload glTF image",
            .type = PassType::Compute,
            .exec =
                [=](RenderGraph & /*graph*/, RenderPass & /*self*/, vulkan::API &api) {
                    auto size = static_cast<usize>(stb_image.width * stb_image.height * stb_image.nb_comp);
                    api.upload_image(image_h, stb_image.pixels, size);
                    api.generate_mipmaps(image_h);

                    stbi_image_free(stb_image.pixels);
                },
        });
    }

    /// --- Create programs

    vulkan::DepthState depth_state = {
        .test         = VK_COMPARE_OP_EQUAL,
        .enable_write = false,
    };

    if (0)
    {
    pass.shading = api.create_program({
        .vertex_shader   = api.create_shader("shaders/gltf.vert.spv"),
        .fragment_shader = api.create_shader("shaders/gltf.frag.spv"),
        .depth           = depth_state,
    });
    }

    pass.shading_simple = api.create_program({
        .vertex_shader   = api.create_shader("shaders/gltf.vert.spv"),
        .fragment_shader = api.create_shader("shaders/gltf_simple.frag.spv"),
        .depth           = depth_state,
    });

    depth_state.test         = VK_COMPARE_OP_GREATER_OR_EQUAL;
    depth_state.enable_write = true;

    pass.prepass = api.create_program({
        .vertex_shader   = api.create_shader("shaders/gltf.vert.spv"),
        .fragment_shader = api.create_shader("shaders/gltf_prepass.frag.spv"),
        .depth           = depth_state,
    });

    if (0)
    {
    pass.shadow_cascade_program = api.create_program({
        .vertex_shader   = api.create_shader("shaders/shadowmap.vert.spv"),
        .fragment_shader = api.create_shader("shaders/shadowmap.frag.spv"),
        .depth           = depth_state,
    });
    }

    pass.culling = api.create_program({
            .shader = api.create_shader("shaders/prepare_draw_indirect.comp.glsl.spv")
        });
    pass.compaction = api.create_program({
            .shader = api.create_shader("shaders/draw_call_compaction.comp.glsl.spv")
        });


    return pass;
}

static void draw_model(vulkan::API &api, Model &model, vulkan::GraphicsProgramH program)
{
    api.bind_program(program);

    u32 i_draw = 0;
    for (auto node_idx : model.nodes_preorder)
    {
        const auto &node = model.nodes[node_idx];
        if (!node.mesh) {
            continue;
        }

        const auto &mesh = model.meshes[*node.mesh];

        // Draw the mesh
        for (auto primitive_idx : mesh.primitives)
        {
            const auto &primitive = model.primitives[primitive_idx];
            GltfPushConstant constants = {.draw_id = i_draw++};
            api.push_constant(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                              0,
                              sizeof(constants),
                              &constants);
            api.draw_indexed(primitive.index_count, 1, primitive.first_index, 0, 0);
        }
    }
}

static void add_shadow_cascades_pass(Renderer &r)
{
    auto &graph = r.graph;
    auto &api   = r.api;

    auto &pass_data     = r.gltf;
    auto &cascades_data = r.cascades_bounds;

    auto external_images = r.gltf.images;

    auto cascades_count = r.settings.shadow_cascades_count;
    for (uint i = 0; i < cascades_count; i++)
    {
        auto cascade_index_pos = api.dynamic_uniform_buffer(sizeof(uint));
        auto *cascade_index    = reinterpret_cast<uint *>(cascade_index_pos.mapped);
        *cascade_index         = i;

        graph.add_pass({
            .name             = "Shadow Cascade",
            .type             = PassType::Graphics,
            .external_images  = external_images,
            .index_buffers    = {pass_data.index_buffer},
            .depth_attachment = r.shadow_cascades[i],
            .exec =
                [=](RenderGraph & /*graph*/, RenderPass & /*self*/, vulkan::API &api) {
                    {
                        auto program = pass_data.shadow_cascade_program;

                        api.bind_buffer(program, pass_data.vertex_buffer, vulkan::SHADER_DESCRIPTOR_SET, 0);
                        api.bind_buffer(program, cascade_index_pos, vulkan::SHADER_DESCRIPTOR_SET, 2);
                        api.bind_buffer(program,
                                        cascades_data.cascades_slices_buffer,
                                        vulkan::SHADER_DESCRIPTOR_SET,
                                        3);
                        api.bind_index_buffer(pass_data.index_buffer);

                        draw_model(api, *pass_data.model, program);
                    }
                },
        });
    }
}

static void add_gltf_prepass(Renderer &r)
{
    auto &graph = r.graph;

    auto &pass_data   = r.gltf;

    auto external_images = r.gltf.images;

    graph.add_pass({
        .name             = "glTF depth prepass",
        .type             = PassType::Graphics,
        .external_images  = external_images,
        .index_buffers    = {pass_data.index_buffer},
        .depth_attachment = r.depth_buffer,
        .exec =
            [=](RenderGraph & /*graph*/, RenderPass & /*self*/, vulkan::API &api) {
                auto program = pass_data.prepass;

                api.bind_buffer(program, pass_data.vertex_buffer, vulkan::SHADER_DESCRIPTOR_SET, 0);
                api.bind_index_buffer(pass_data.index_buffer);

                draw_model(api, *pass_data.model, program);
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

    auto &pass_data     = r.gltf;
    auto &voxel_data    = r.voxels;
    auto &cascades_data = r.cascades_bounds;

    graph.add_pass({
        .name              = "glTF pass",
        .type              = PassType::Graphics,
        .external_images   = external_images,
        .sampled_images    = sampled_images,
        .index_buffers     = {pass_data.index_buffer},
        .color_attachments = {r.hdr_buffer},
        .depth_attachment  = r.depth_buffer,
        .exec =
            [=](RenderGraph &graph, RenderPass &self, vulkan::API &api) {

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
                                                voxels_radiance,
                                                api.trilinear_sampler,
                                                vulkan::SHADER_DESCRIPTOR_SET,
                                                4);

                api.bind_combined_images_samplers(program,
                                                  voxels_directional_volumes,
                                                  {api.trilinear_sampler},
                                                  vulkan::SHADER_DESCRIPTOR_SET,
                                                  5);

                api.bind_combined_images_samplers(program,
                                                  shadow_cascades,
                                                  {api.trilinear_sampler},
                                                  vulkan::SHADER_DESCRIPTOR_SET,
                                                  6);

                api.bind_buffer(program, cascades_data.cascades_slices_buffer, vulkan::SHADER_DESCRIPTOR_SET, 7);

                api.bind_index_buffer(pass_data.index_buffer);

                draw_model(api, *pass_data.model, program);
            },
    });
}

static void add_gltf_simple_prepass(Renderer &r)
{
    auto &graph = r.graph;

    auto &pass_data   = r.gltf;

    graph.add_pass({
        .name             = "clear final draw commands / data and visibility",
        .type             = PassType::Compute,
        .transfer_dst_buffers = {pass_data.finaldata_buffer, pass_data.finalcommands_buffer, pass_data.visibility_buffer},
        .exec =
            [=](RenderGraph & /*graph*/, RenderPass & /*self*/, vulkan::API &api) {
                api.clear_buffer(pass_data.finaldata_buffer, 0u);
                api.clear_buffer(pass_data.finalcommands_buffer, 0u);
                api.clear_buffer(pass_data.visibility_buffer, 0u);
            },
    });

    graph.add_pass({
        .name             = "frustum culling",
        .type             = PassType::Compute,
        .storage_buffers  = {pass_data.commands_buffer,pass_data.draws_buffer,pass_data.primitives_buffer,pass_data.transforms_buffer,pass_data.visibility_buffer},
        .exec =
            [=](RenderGraph & /*graph*/, RenderPass & /*self*/, vulkan::API &api) {
                auto program = pass_data.culling;
                api.bind_buffer(program, pass_data.commands_buffer, 0);
                api.bind_buffer(program, pass_data.draws_buffer, 1);
                api.bind_buffer(program, pass_data.primitives_buffer, 2);
                api.bind_buffer(program, pass_data.transforms_buffer, 3);
                api.bind_buffer(program, pass_data.visibility_buffer, 4);
                api.dispatch(program, {64, 1, 1});
            },
    });

    graph.add_pass({
        .name             = "draw compaction",
        .type             = PassType::Compute,
        .storage_buffers  = {pass_data.commands_buffer,pass_data.draws_buffer,pass_data.visibility_buffer, pass_data.finaldata_buffer, pass_data.finalcommands_buffer},
        .exec =
            [=](RenderGraph & /*graph*/, RenderPass & /*self*/, vulkan::API &api) {
                auto program = pass_data.compaction;
                api.bind_buffer(program, pass_data.commands_buffer, 0);
                api.bind_buffer(program, pass_data.draws_buffer, 1);
                api.bind_buffer(program, pass_data.visibility_buffer, 2);
                api.bind_buffer(program, pass_data.finalcommands_buffer, 3);
                api.bind_buffer(program, pass_data.finaldata_buffer, 4);
                api.dispatch(program, {1, 1, 1});
            },
    });

    graph.add_pass({
        .name             = "glTF simple depth prepass",
        .type             = PassType::Graphics,
        .storage_buffers  = {pass_data.vertex_buffer,pass_data.material_buffer,pass_data.finaldata_buffer, pass_data.transforms_buffer},
        .depth_attachment = r.depth_buffer,
        .exec =
            [=](RenderGraph & /*graph*/, RenderPass & /*self*/, vulkan::API &api) {
                auto program = pass_data.prepass;

                api.bind_buffer(program, pass_data.vertex_buffer, vulkan::SHADER_DESCRIPTOR_SET, 0);
                api.bind_buffer(program, pass_data.material_buffer, vulkan::SHADER_DESCRIPTOR_SET, 1);
                api.bind_buffer(program, pass_data.finaldata_buffer, vulkan::SHADER_DESCRIPTOR_SET, 2);
                api.bind_buffer(program, pass_data.transforms_buffer, vulkan::SHADER_DESCRIPTOR_SET, 3);

                api.bind_index_buffer(pass_data.index_buffer);

                api.bind_program(program);
                api.draw_indexed_indirect(pass_data.commands_buffer, sizeof(u32), pass_data.finalcommands_buffer, 0, pass_data.commands.draw_count);
            },
    });
}


static void add_gltf_simple_pass(Renderer &r)
{
    auto &graph = r.graph;

    auto &pass_data     = r.gltf;

    graph.add_pass({
        .name              = "glTF simplified pass",
        .type              = PassType::Graphics,
        .color_attachments = {r.hdr_buffer},
        .depth_attachment  = r.depth_buffer,
        .exec =
        [=](RenderGraph &/*graph*/, RenderPass &/*self*/, vulkan::API &api) {

                auto program = pass_data.shading_simple;

                api.bind_buffer(program, pass_data.vertex_buffer, vulkan::SHADER_DESCRIPTOR_SET, 0);
                api.bind_buffer(program, pass_data.material_buffer, vulkan::SHADER_DESCRIPTOR_SET, 1);
                api.bind_buffer(program, pass_data.finaldata_buffer, vulkan::SHADER_DESCRIPTOR_SET, 2);
                api.bind_buffer(program, pass_data.transforms_buffer, vulkan::SHADER_DESCRIPTOR_SET, 3);

                api.bind_index_buffer(pass_data.index_buffer);
                api.bind_program(program);
                api.draw_indexed_indirect(pass_data.commands_buffer, sizeof(u32), pass_data.finalcommands_buffer, 0, pass_data.commands.draw_count);
            },
    });
}

/// --- Voxels

Renderer::VoxelPass create_voxel_pass(vulkan::API &api)
{
    Renderer::VoxelPass pass;

    vulkan::RasterizationState rasterization = {.culling = false};
    if (0)
    {
    pass.voxelization = api.create_program({
        .vertex_shader   = api.create_shader("shaders/voxelization.vert.spv"),
        .geom_shader     = api.create_shader("shaders/voxelization.geom.spv"),
        .fragment_shader = api.create_shader("shaders/voxelization.frag.spv"),
        .rasterization   = rasterization,
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
    }

    return pass;
}

static void add_voxels_clear_pass(Renderer &r)
{
    auto &graph         = r.graph;
    auto &voxel_options = r.voxel_options;
    auto &pass_data     = r.voxels;

    graph.add_pass({
        .name           = "Voxels clear",
        .type           = PassType::Compute,
        .storage_images = {r.voxels_albedo, r.voxels_normal, r.voxels_radiance},
        .exec =
            [=](RenderGraph &graph, RenderPass &self, vulkan::API &api) {
                auto voxels_albedo   = graph.get_resolved_image(self.storage_images[0]);
                auto voxels_normal   = graph.get_resolved_image(self.storage_images[1]);
                auto voxels_radiance = graph.get_resolved_image(self.storage_images[2]);

                auto program = pass_data.clear_voxels;

                api.bind_buffer(program, pass_data.voxel_options_pos, 0);
                api.bind_image(program, voxels_albedo, 1);
                api.bind_image(program, voxels_normal, 2);
                api.bind_image(program, voxels_radiance, 3);

                auto count = voxel_options.res / 8;
                api.dispatch(program, {count, count, count});
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

    auto &model_data    = r.gltf;
    auto &voxel_options = r.voxel_options;

    graph.add_pass({
        .name           = "Voxelization",
        .type           = PassType::Graphics,
        .storage_images = {r.voxels_albedo, r.voxels_normal},
        .samples        = VK_SAMPLE_COUNT_32_BIT,
        .exec =
            [=](RenderGraph &graph, RenderPass &self, vulkan::API &api) {
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

                draw_model(api, *model_data.model, program);
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

    auto &pass_data = r.voxels;

    graph.add_pass({
        .name              = "Voxels debug visualization",
        .type              = PassType::Graphics,
        .sampled_images    = {voxels},
        .color_attachments = {r.hdr_buffer},
        .depth_attachment  = r.depth_buffer,
        .exec =
            [=](RenderGraph &graph, RenderPass &self, vulkan::API &api) {
                auto voxels = graph.get_resolved_image(self.sampled_images[0]);

                auto program = pass_data.debug_visualization;

                api.bind_buffer(program, options_pos, vulkan::SHADER_DESCRIPTOR_SET, 0);
                api.bind_buffer(program, pass_data.vct_debug_pos, vulkan::SHADER_DESCRIPTOR_SET, 1);

                api.bind_combined_image_sampler(program, voxels, api.trilinear_sampler, vulkan::SHADER_DESCRIPTOR_SET, 2);

                api.bind_program(program);

                api.draw(vertex_count, 1, 0, 0);
            },
    });
}

static void add_voxels_direct_lighting_pass(Renderer &r)
{
    auto &graph = r.graph;

    auto &pass_data = r.voxels;
    auto &voxel_options = r.voxel_options;

    graph.add_pass({
        .name           = "Voxels direct lighting",
        .type           = PassType::Compute,
        .sampled_images = {r.voxels_albedo, r.voxels_normal},
        .storage_images = {r.voxels_radiance},
        .exec =
            [=](RenderGraph &graph, RenderPass &self, vulkan::API &api) {
                auto voxels_albedo = graph.get_resolved_image(self.sampled_images[0]);
                auto voxels_normal = graph.get_resolved_image(self.sampled_images[1]);
                auto voxels_radiance = graph.get_resolved_image(self.storage_images[0]);

                const auto &program = pass_data.inject_radiance;

                api.bind_buffer(program, pass_data.voxel_options_pos, 0);
                api.bind_buffer(program, pass_data.vct_debug_pos, 1);
                api.bind_combined_image_sampler(program,
                                                voxels_albedo,
                                                api.trilinear_sampler,
                                                2);
                api.bind_combined_image_sampler(program,
                                                voxels_normal,
                                                api.trilinear_sampler,
                                                3);
                api.bind_image(program, voxels_radiance, 4);

                auto count = voxel_options.res / 8;
                api.dispatch(program, {count, count, count});
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
    auto &pass_data = r.voxels;

    graph.add_pass({
        .name           = "Voxels aniso base",
        .type           = PassType::Compute,
        .sampled_images = {r.voxels_radiance},
        .storage_images = storage_images,
        .exec =
            [=](RenderGraph &graph,
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
                                                voxels_radiance,
                                                trilinear_sampler,
                                                1);
                api.bind_images(program, views, 2);

                api.dispatch(program, {count, count, count});
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
                [=](RenderGraph &graph,
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

                    api.dispatch(program, {count, count, count});
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


    auto &api = r.api;
    r.global_uniform_pos = api.dynamic_uniform_buffer(sizeof(GlobalUniform));
    auto *globals        = reinterpret_cast<GlobalUniform *>(r.global_uniform_pos.mapped);
    std::memset(globals, 0, sizeof(GlobalUniform));

    globals->camera_previous_view = camera.view;
    globals->camera_previous_proj = camera.projection;

    float aspect_ratio = r.settings.render_resolution.x / float(r.settings.render_resolution.y);
    camera.view = camera::look_at(camera_transform.position, input_camera.target, float3_UP, &camera.view_inverse);
    camera.projection  = camera::perspective(camera.fov,
                                            aspect_ratio,
                                            camera.near_plane,
                                            camera.far_plane,
                                            &camera.projection_inverse);

    globals->delta_t              = r.p_timer->get_delta_time();
    globals->camera_pos           = camera_transform.position;
    globals->camera_view          = camera.view;
    globals->camera_inv_view      = camera.view_inverse;
    globals->camera_proj          = camera.projection;
    globals->camera_inv_proj      = camera.projection_inverse;
    globals->camera_inv_view_proj = camera.view_inverse * camera.projection_inverse;


    globals->camera_near          = camera.near_plane;
    globals->camera_far           = camera.far_plane;

    /// Compute TAA offset
    float2 current_sample = r.halton_indices[(api.ctx.frame_count+1)%r.halton_indices.size()];
    if (!r.settings.enable_taa)
    {
        current_sample = float2(0.0);
    }

    float2 view_rect = float2(
        2.0f * camera.near_plane / camera.projection.at(0, 0),
        -2.0f * camera.near_plane / camera.projection.at(1, 1)
        );

    float2 texel_size = float2(
        view_rect.x / r.settings.render_resolution.x,
        view_rect.y / r.settings.render_resolution.y
        );

    globals->previous_jitter_offset      = r.previous_jitter;

    globals->jitter_offset.x      = (current_sample.x * texel_size.x) / view_rect.x;
    globals->jitter_offset.y      = (current_sample.y * texel_size.y) / view_rect.y;

    r.previous_jitter = globals->jitter_offset;


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

    std::vector<vulkan::ImageH> images;
    std::vector<vulkan::SamplerH> samplers;

    for (uint i = 0; i < r.gltf.model->textures.size(); i++)
    {
        const auto &texture = r.gltf.model->textures[i];
        images.push_back(r.gltf.images[texture.image]);
        samplers.push_back(r.gltf.samplers[texture.sampler]);
    }

    r.random_rotation_idx = images.size();
    images.push_back(r.random_rotations);
    samplers.push_back(r.nearest_repeat_sampler);

    r.graph.add_pass({
        .name            = "Bind global images",
        .type            = PassType::Compute,
        .external_images = images,
        .exec =
            [=](RenderGraph & /*graph*/, RenderPass & /*self*/, vulkan::API & api) {
                api.bind_combined_images_samplers({}, images, samplers, vulkan::GLOBAL_DESCRIPTOR_SET, 1);
                api.update_global_set();
            },
    });

    r.voxels.vct_debug_pos = api.dynamic_uniform_buffer(sizeof(VCTDebug));
    auto *debug            = reinterpret_cast<VCTDebug *>(r.voxels.vct_debug_pos.mapped);
    *debug                 = r.vct_debug;

    r.voxels.voxel_options_pos = api.dynamic_uniform_buffer(sizeof(VoxelOptions));
    auto *buffer0              = reinterpret_cast<VoxelOptions *>(r.voxels.voxel_options_pos.mapped);
    *buffer0                   = r.voxel_options;
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
            ImGui::Checkbox("Enable TAA", &settings.enable_taa);
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
            for (auto shadow_map : shadow_cascades)
            {
                auto &res = graph.images.at(shadow_map);
                if (res.resolved_img.is_valid())
                {
                    ImGui::Image((void *)(api.get_image(res.resolved_img).default_view.hash()), ImVec2(512, 512));
                }
            }
        }

        if (ImGui::CollapsingHeader("TAA history"))
        {
            for (auto h : history)
            {
                auto &res = graph.images.at(h);
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
                ImGui::Checkbox("Show cascades", reinterpret_cast<bool*>(&vct_debug.show_cascades));
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
            tools::imgui_select("Tonemap", options.data(), options.size(), tonemapping.debug.selected);
            ImGui::SliderFloat("Exposure", &tonemapping.debug.exposure, 0.0f, 2.0f);
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

static void create_graph_images(Renderer &r)
{
    r.depth_buffer = r.graph.image_descs.add({.name = "Depth Buffer", .format = VK_FORMAT_D32_SFLOAT});
    r.hdr_buffer = r.graph.image_descs.add({.name = "HDR Buffer", .format = VK_FORMAT_R16G16B16A16_SFLOAT});
    r.ldr_buffer = r.graph.image_descs.add({.name = "LDR Buffer", .format = r.api.ctx.swapchain.format.format});

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

    r.history[0] = r.graph.image_descs.add({.name = "History 0", .format = VK_FORMAT_R16G16B16A16_SFLOAT});
    r.history[1] = r.graph.image_descs.add({.name = "History 1", .format = VK_FORMAT_R16G16B16A16_SFLOAT});
    r.taa_output = r.graph.image_descs.add({.name = "TAA output", .format = VK_FORMAT_R16G16B16A16_SFLOAT});
}

void add_accumulation_pass(Renderer &r)
{
    auto &graph = r.graph;


    /// Render graph hack: the render graph assign VkImage to each resource every frame based on the resource's usage this frame
    /// The TAA history buffers are used as sampled image one frame and storage image the other so the render graph
    /// will swap them every frame...
    /// To make it work these 2 passes fix their usages to sampled AND storage.
    /// Because they are created in the same order every frame, they will always have the same VkImage
    /// TODO: Fix this mess ^^"

    graph.add_pass({
            .name           = "Prepare history",
            .type           = PassType::Compute,
            .sampled_images = {r.history[0], r.history[1]},
            .exec =
            [](RenderGraph &/*graph*/, RenderPass &/*self*/, vulkan::API &/*api*/) {},
        });
    graph.add_pass({
            .name           = "Prepare history 2",
            .type           = PassType::Compute,
            .storage_images = {r.history[0], r.history[1]},
            .exec =
            [](RenderGraph &/*graph*/, RenderPass &/*self*/, vulkan::API &/*api*/) {},
        });




    auto prev = r.history[(r.current_history)%2];
    r.current_history += 1;
    auto next = r.history[(r.current_history)%2];

    graph.add_pass({
        .name           = "Temporal accumulation",
        .type           = PassType::Compute,
        .sampled_images = {r.depth_buffer, r.hdr_buffer, prev},
        .storage_images = {next},
        .exec =
            [=](RenderGraph &graph, RenderPass &self, vulkan::API &api) {

                auto program    = r.temporal_pass.accumulate;

                auto depth_buffer = graph.get_resolved_image(self.sampled_images[0]);
                auto hdr_buffer = graph.get_resolved_image(self.sampled_images[1]);
                auto prev_history = graph.get_resolved_image(self.sampled_images[2]);
                auto taa_output = graph.get_resolved_image(self.storage_images[0]);

                api.bind_combined_image_sampler(program,
                                                depth_buffer,
                                                r.nearest_sampler,
                                                0);

                api.bind_combined_image_sampler(program,
                                                hdr_buffer,
                                                r.nearest_sampler,
                                                1);

                api.bind_combined_image_sampler(program,
                                                prev_history,
                                                r.nearest_sampler,
                                                2);

                api.bind_image(program, taa_output, 3);

                api.dispatch(program, api.dispatch_size(hdr_buffer, 16));
            },
    });
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
    graph.start_frame();

    create_graph_images(*this);

    update_uniforms(*this, world, main_camera);

    constexpr bool SIMPLE = true;

    if (!SIMPLE)
    {
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
            // main pass

            add_gltf_prepass(*this);
            add_cascades_bounds_pass(*this);
            add_shadow_cascades_pass(*this);

            add_gltf_pass(*this);
        }
    }
    else
    {
        add_gltf_simple_prepass(*this);
        add_gltf_simple_pass(*this);
    }

    auto *sky_atmosphere = world.singleton_get_component<SkyAtmosphereComponent>();
    if (sky_atmosphere)
    {
        add_procedural_sky_pass(graph, procedural_sky, *sky_atmosphere, depth_buffer, hdr_buffer);
    }

    if (settings.enable_taa)
    {
        add_accumulation_pass(*this);
        override_main_pass_output = &history[(current_history%2)];
    }
    else
    {
        override_main_pass_output = nullptr;
    }

    // Post processes
    auto output = override_main_pass_output ? *override_main_pass_output : hdr_buffer;

    add_luminance_pass(graph, luminance, output);

    add_tonemapping_pass(graph, tonemapping, luminance, output, ldr_buffer);

    if (settings.show_grid)
    {
        add_floor_pass(*this);
    }

    add_imgui_pass(graph, imgui, graph.swapchain);

    ImGui::EndFrame(); // right before drawing the ui

    if (!graph.execute())
    {
        return;
    }

    api.end_frame();
}

} // namespace my_app
