#include "renderer/renderer.hpp"
#include "app.hpp"
#include "camera.hpp"
#include "renderer/hl_api.hpp"
#include "tools.hpp"
#include "window.hpp"
#include <vulkan/vulkan.hpp>
#if defined(ENABLE_IMGUI)
#include <imgui.h>
#endif
#include "file_watcher.hpp"
#include "types.hpp"
#include "timer.hpp"
#include <iostream>
#include <fstream>

#include "../shaders/include/atmosphere.h"

// #define ENABLE_SPARSE

namespace my_app
{

struct TileAllocationRequest
{
    vk::Offset3D offset;
    u32 mip_level;
};


struct ShaderDebug
{
    uint selected;
    float opacity;
};

Renderer Renderer::create(const Window &window, Camera &camera, TimerData &timer, UI::Context &ui)
{
    Renderer r;
    r.api      = vulkan::API::create(window);
    auto &api = r.api;

    r.p_ui     = &ui;
    r.p_window = &window;
    r.p_camera = &camera;
    r.p_timer  = &timer;

    /// --- Setup basic attachments

    {
        vulkan::ImageInfo iinfo;
        iinfo.name   = "Depth";
        iinfo.format = vk::Format::eD32Sfloat;
        iinfo.width  = api.ctx.swapchain.extent.width;
        iinfo.height = api.ctx.swapchain.extent.height;
        iinfo.depth  = 1;
        iinfo.usages = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled;
        auto depth_h = api.create_image(iinfo);

        vulkan::RTInfo dinfo;
        dinfo.is_swapchain = false;
        dinfo.image_h      = depth_h;
        r.depth_rt         = api.create_rendertarget(dinfo);
    }

    {
        vulkan::ImageInfo iinfo;
        iinfo.name   = "HDR color";
        iinfo.format = vk::Format::eR16G16B16A16Sfloat;
        iinfo.width  = api.ctx.swapchain.extent.width;
        iinfo.height = api.ctx.swapchain.extent.height;
        iinfo.depth  = 1;
        iinfo.usages = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
        auto color_h = api.create_image(iinfo);

        vulkan::RTInfo cinfo;
        cinfo.is_swapchain = false;
        cinfo.image_h      = color_h;
        r.color_rt         = api.create_rendertarget(cinfo);
    }

    {
        vulkan::RTInfo sinfo;
        sinfo.is_swapchain = true;
        r.swapchain_rt = api.create_rendertarget(sinfo);
    }


    {
        vulkan::GraphicsProgramInfo pinfo{};
        pinfo.vertex_shader   = api.create_shader("shaders/fullscreen_triangle.vert.spv");
        pinfo.fragment_shader = api.create_shader("shaders/hdr_compositing.frag.spv");

        pinfo.binding({.set =  vulkan::SHADER_DESCRIPTOR_SET, .slot =  0,
                       .stages =  vk::ShaderStageFlagBits::eFragment,
                       .type =  vk::DescriptorType::eCombinedImageSampler, .count =  1});

        pinfo.binding({.set =  vulkan::SHADER_DESCRIPTOR_SET, .slot =  1,
                       .stages =  vk::ShaderStageFlagBits::eFragment,
                       .type =  vk::DescriptorType::eUniformBufferDynamic, .count =  1});

        r.hdr_compositing = api.create_program(std::move(pinfo));
    }

    {
        vulkan::SamplerInfo sinfo{};
        r.default_sampler = api.create_sampler(sinfo);
    }

    /// --- Init ImGui

#if defined(ENABLE_IMGUI)
    {
        ImGui::CreateContext();
        /*
        auto &style             = ImGui::GetStyle();
        style.FrameRounding     = 0.f;
        style.GrabRounding      = 0.f;
        style.WindowRounding    = 0.f;
        style.ScrollbarRounding = 0.f;
        style.GrabRounding      = 0.f;
        style.TabRounding       = 0.f;
        */

        auto &io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigDockingWithShift = false;
        io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;          // We can honor io.WantSetMousePos requests (optional, rarely used)
        io.BackendPlatformName = "custom_glfw";
    }

    {
        vulkan::GraphicsProgramInfo pinfo{};
        pinfo.vertex_shader   = api.create_shader("shaders/gui.vert.spv");
        pinfo.fragment_shader = api.create_shader("shaders/gui.frag.spv");
        // clang-format off
        pinfo.push_constant({.stages =  vk::ShaderStageFlagBits::eVertex, .offset =  0, .size =  4 * sizeof(float)});
        pinfo.binding({.set =  vulkan::SHADER_DESCRIPTOR_SET, .slot =  0, .stages =  vk::ShaderStageFlagBits::eFragment, .type =  vk::DescriptorType::eCombinedImageSampler, .count =  1});
        // clang-format on
        pinfo.vertex_stride(sizeof(ImDrawVert));

        pinfo.vertex_info({.format = vk::Format::eR32G32Sfloat, .offset = MEMBER_OFFSET(ImDrawVert, pos)});
        pinfo.vertex_info({.format = vk::Format::eR32G32Sfloat, .offset = MEMBER_OFFSET(ImDrawVert, uv)});
        pinfo.vertex_info({.format = vk::Format::eR8G8B8A8Unorm,.offset =  MEMBER_OFFSET(ImDrawVert, col)});

        vulkan::GraphicsProgramInfo puintinfo = pinfo;
        puintinfo.fragment_shader = api.create_shader("shaders/gui_uint.frag.spv");

        r.gui_program = api.create_program(std::move(pinfo));
        r.gui_uint_program = api.create_program(std::move(puintinfo));
    }

    {
        vulkan::ImageInfo iinfo;
        iinfo.name = "ImGui font texture";

        uchar *pixels = nullptr;

        // Get image data
        int w = 0;
        int h = 0;
        ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);

        iinfo.width  = static_cast<u32>(w);
        iinfo.height = static_cast<u32>(h);
        iinfo.depth  = 1;

        r.gui_texture = api.create_image(iinfo);
        api.upload_image(r.gui_texture, pixels, iinfo.width * iinfo.height * 4);
    }
#endif

    /// --- glTF Model

    {
        r.model = load_model("../models/Sponza/glTF/Sponza.gltf");
        r.load_model_data();
    }

    /// --- Sparse Shadow Map

    {
        vulkan::GraphicsProgramInfo pinfo{};
        pinfo.vertex_shader = api.create_shader("shaders/gltf.vert.spv");
        pinfo.fragment_shader = api.create_shader("shaders/gltf_prepass.frag.spv");

        // camera uniform buffer
        pinfo.binding({.set    = vulkan::GLOBAL_DESCRIPTOR_SET,
                       .slot   = 0,
                       .stages = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eCompute,
                       .type   = vk::DescriptorType::eUniformBufferDynamic,
                       .count  = 1});

        // node transform
        pinfo.binding({.set    = vulkan::DRAW_DESCRIPTOR_SET,
                       .slot   = 0,
                       .stages = vk::ShaderStageFlagBits::eVertex,
                       .type   = vk::DescriptorType::eUniformBufferDynamic,
                       .count  = 1});

        // base color texture
        pinfo.binding({.set    = vulkan::DRAW_DESCRIPTOR_SET,
                       .slot   = 1,
                       .stages = vk::ShaderStageFlagBits::eFragment,
                       .type   = vk::DescriptorType::eCombinedImageSampler,
                       .count  = 1});

        pinfo.vertex_stride(sizeof(GltfVertex));
        pinfo.vertex_info({vk::Format::eR32G32B32Sfloat, MEMBER_OFFSET(GltfVertex, position)});
        pinfo.vertex_info({vk::Format::eR32G32B32Sfloat, MEMBER_OFFSET(GltfVertex, normal)});
        pinfo.vertex_info({vk::Format::eR32G32Sfloat, MEMBER_OFFSET(GltfVertex, uv0)});
        pinfo.vertex_info({vk::Format::eR32G32Sfloat, MEMBER_OFFSET(GltfVertex, uv1)});
        pinfo.vertex_info({vk::Format::eR32G32B32A32Sfloat, MEMBER_OFFSET(GltfVertex, joint0)});
        pinfo.vertex_info({vk::Format::eR32G32B32A32Sfloat, MEMBER_OFFSET(GltfVertex, weight0)});
        pinfo.depth_test = vk::CompareOp::eGreaterOrEqual;
        pinfo.enable_depth_write = true;

        r.model_prepass = api.create_program(std::move(pinfo));
    }

    {
        vulkan::ComputeProgramInfo pinfo{};
        pinfo.shader = api.create_shader("shaders/min_lod_map.comp.spv");

        // camera uniform buffer
        pinfo.binding({.slot   = 0,
                       .stages = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eCompute,
                       .type   = vk::DescriptorType::eUniformBufferDynamic,
                       .count  = 1});

        // screen-space lod
        pinfo.binding({.slot   = 1,
                       .stages = vk::ShaderStageFlagBits::eCompute,
                       .type   = vk::DescriptorType::eCombinedImageSampler,
                       .count  = 1});

        // depth buffer to reconstruct world position from screen
        pinfo.binding({.slot   = 2,
                       .stages = vk::ShaderStageFlagBits::eCompute,
                       .type   = vk::DescriptorType::eCombinedImageSampler,
                       .count  = 1});

        // min lod map
        pinfo.binding({.slot   = 3,
                       .stages = vk::ShaderStageFlagBits::eCompute,
                       .type   = vk::DescriptorType::eStorageImage,
                       .count  = 1});

        r.fill_min_lod_map = api.create_program(std::move(pinfo));
    }

    {
        vulkan::ImageInfo iinfo;
        iinfo.name   = "Shadow Map LOD";
        iinfo.format = vk::Format::eR8Uint;
        iinfo.width  = api.ctx.swapchain.extent.width;
        iinfo.height = api.ctx.swapchain.extent.height;
        iinfo.depth  = 1;
        iinfo.usages = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
        auto lod_map_h = api.create_image(iinfo);

        vulkan::RTInfo cinfo;
        cinfo.is_swapchain     = false;
        cinfo.image_h          = lod_map_h;
        r.screenspace_lod_map_rt = api.create_rendertarget(cinfo);
    }

    {
        vulkan::ImageInfo sm_info;
        sm_info.name   = "Sparse Shadow map";
        sm_info.format = vk::Format::eD32Sfloat;
        sm_info.width  = 16 * 1024;
        sm_info.height = 16 * 1024;
        sm_info.depth  = 1;
        sm_info.mip_levels = 8;
        sm_info.usages = vk::ImageUsageFlagBits::eDepthStencilAttachment;
        sm_info.is_sparse = true;
        sm_info.max_sparse_size = 64u * 1024u * 1024u; // 64Mb should be the size of a 4K non-sparse shadow map

#if defined(ENABLE_SPARSE)
        auto shadow_map_h = api.create_image(sm_info);

        vulkan::RTInfo rt_info;
        rt_info.is_swapchain     = false;
        rt_info.image_h          = shadow_map_h;
        r.shadow_map_rt = api.create_rendertarget(rt_info);
#endif

        vulkan::ImageInfo mlm_info;
        mlm_info.name      = "ShadowMap Min LOD";
        mlm_info.format    = vk::Format::eR32Uint; // needed for atomic even though 8 bits should be enough...
        mlm_info.width     = sm_info.width / 128; // https://renderdoc.org/vkspec_chunked/chap32.html#sparsememory-standard-shapes
        mlm_info.height    = sm_info.height / 128; // standard block shape for 32 bits / texel 2D texture is 128x128
        mlm_info.depth     = 1;
        mlm_info.usages    = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
        mlm_info.memory_usage = VMA_MEMORY_USAGE_GPU_TO_CPU;
        mlm_info.is_linear = true;

        r.min_lod_map_per_frame.resize(vulkan::FRAMES_IN_FLIGHT);

        for (auto &copy : r.min_lod_map_per_frame)
        {
            copy = api.create_image(mlm_info);
        }
    }

    /// --- Voxelization
    {
        r.voxel_options.res = 256;

        vulkan::ImageInfo iinfo;
        iinfo.name         = "Voxels albedo";
        iinfo.type         = vk::ImageType::e3D;
        iinfo.format       = vk::Format::eR8G8B8A8Unorm;
        iinfo.extra_formats = {vk::Format::eR32Uint};
        iinfo.width        = r.voxel_options.res;
        iinfo.height       = r.voxel_options.res;
        iinfo.depth        = r.voxel_options.res;
        iinfo.usages       = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferDst;
        r.voxels_albedo    = api.create_image(iinfo);

        iinfo.name         = "Voxels normal";
        r.voxels_normal    = api.create_image(iinfo);

        iinfo.name          = "Voxels radiance";
        iinfo.format        = vk::Format::eR16G16B16A16Sfloat;
        iinfo.extra_formats = {};
        r.voxels_radiance   = api.create_image(iinfo);

        vulkan::SamplerInfo sinfo{};
        sinfo.mag_filter   = vk::Filter::eLinear;
        sinfo.min_filter   = vk::Filter::eLinear;
        sinfo.mip_map_mode = vk::SamplerMipmapMode::eLinear;
        sinfo.address_mode = vk::SamplerAddressMode::eClampToBorder;
        r.trilinear_sampler  = api.create_sampler(sinfo);

        sinfo.mag_filter   = vk::Filter::eNearest;
        sinfo.min_filter   = vk::Filter::eNearest;
        sinfo.mip_map_mode = vk::SamplerMipmapMode::eNearest;
        r.nearest_sampler  = api.create_sampler(sinfo);
    }
    // voxels directional volumes
    {
        r.voxels_directional_volumes.resize(6);

        u32 size = r.voxel_options.res / 2;

        vulkan::ImageInfo iinfo;
        iinfo.type         = vk::ImageType::e3D;
        iinfo.format       = vk::Format::eR16G16B16A16Sfloat;
        iinfo.width        = size;
        iinfo.height       = size;
        iinfo.depth        = size;
        iinfo.mip_levels   = static_cast<u32>(std::floor(std::log2(size)) + 1.0);
        iinfo.usages       = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferDst;

        iinfo.name                         = "Voxels directional volume -X";
        r.voxels_directional_volumes[0]    = api.create_image(iinfo);
        iinfo.name                         = "Voxels directional volume +X";
        r.voxels_directional_volumes[1]    = api.create_image(iinfo);
        iinfo.name                         = "Voxels directional volume -Y";
        r.voxels_directional_volumes[2]    = api.create_image(iinfo);
        iinfo.name                         = "Voxels directional volume +Y";
        r.voxels_directional_volumes[3]    = api.create_image(iinfo);
        iinfo.name                         = "Voxels directional volume -Z";
        r.voxels_directional_volumes[4]    = api.create_image(iinfo);
        iinfo.name                         = "Voxels directional volume +Z";
        r.voxels_directional_volumes[5]    = api.create_image(iinfo);
    }

    {
        vulkan::GraphicsProgramInfo pinfo{};
        pinfo.vertex_shader   = api.create_shader("shaders/voxelization.vert.spv");
        pinfo.geom_shader     = api.create_shader("shaders/voxelization.geom.spv");
        pinfo.fragment_shader = api.create_shader("shaders/voxelization.frag.spv");

        // voxel options
        pinfo.binding({.set =  vulkan::SHADER_DESCRIPTOR_SET, .slot =  0,
                       .stages =  vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment,
                       .type =  vk::DescriptorType::eUniformBufferDynamic, .count =  1});

        // projection cameras
        pinfo.binding({.set =  vulkan::SHADER_DESCRIPTOR_SET, .slot =  1,
                       .stages =  vk::ShaderStageFlagBits::eGeometry,
                       .type =  vk::DescriptorType::eUniformBufferDynamic, .count =  1});

        // voxels textures
        pinfo.binding({.set =  vulkan::SHADER_DESCRIPTOR_SET, .slot =  2,
                       .stages =  vk::ShaderStageFlagBits::eFragment,
                       .type =  vk::DescriptorType::eStorageImage, .count =  1});
        pinfo.binding({.set =  vulkan::SHADER_DESCRIPTOR_SET, .slot =  3,
                       .stages =  vk::ShaderStageFlagBits::eFragment,
                       .type =  vk::DescriptorType::eStorageImage, .count =  1});

        // node transform
        pinfo.binding({.set =  vulkan::DRAW_DESCRIPTOR_SET, .slot =  0,
                       .stages =  vk::ShaderStageFlagBits::eVertex,
                       .type =  vk::DescriptorType::eUniformBufferDynamic, .count =  1});

        // color texture
        pinfo.binding({.set =  vulkan::DRAW_DESCRIPTOR_SET, .slot =  1,
                       .stages =  vk::ShaderStageFlagBits::eFragment,
                       .type =  vk::DescriptorType::eCombinedImageSampler, .count =  1});

        // normal texture
        pinfo.binding({.set =  vulkan::DRAW_DESCRIPTOR_SET, .slot =  2,
                       .stages =  vk::ShaderStageFlagBits::eFragment,
                       .type =  vk::DescriptorType::eCombinedImageSampler, .count =  1});

        pinfo.vertex_stride(sizeof(GltfVertex));
        pinfo.vertex_info({vk::Format::eR32G32B32Sfloat, MEMBER_OFFSET(GltfVertex, position)});
        pinfo.vertex_info({vk::Format::eR32G32B32Sfloat, MEMBER_OFFSET(GltfVertex, normal)});
        pinfo.vertex_info({vk::Format::eR32G32Sfloat, MEMBER_OFFSET(GltfVertex, uv0)});
        pinfo.vertex_info({vk::Format::eR32G32Sfloat, MEMBER_OFFSET(GltfVertex, uv1)});
        pinfo.vertex_info({vk::Format::eR32G32B32A32Sfloat, MEMBER_OFFSET(GltfVertex, joint0)});
        pinfo.vertex_info({vk::Format::eR32G32B32A32Sfloat, MEMBER_OFFSET(GltfVertex, weight0)});

        pinfo.enable_conservative_rasterization = true;

        r.voxelization = api.create_program(std::move(pinfo));
    }

    {
        vulkan::GraphicsProgramInfo pinfo{};
        pinfo.vertex_shader   = api.create_shader("shaders/fullscreen_triangle.vert.spv");
        pinfo.fragment_shader = api.create_shader("shaders/voxel_visualization.frag.spv");

        // voxel options
        pinfo.binding({.set =  vulkan::SHADER_DESCRIPTOR_SET, .slot =  0,
                       .stages =  vk::ShaderStageFlagBits::eFragment,
                       .type =  vk::DescriptorType::eUniformBufferDynamic, .count =  1});
        // camera
        pinfo.binding({.set =  vulkan::SHADER_DESCRIPTOR_SET, .slot =  1,
                       .stages =  vk::ShaderStageFlagBits::eFragment,
                       .type =  vk::DescriptorType::eUniformBufferDynamic, .count =  1});
        // debug
        pinfo.binding({.set =  vulkan::SHADER_DESCRIPTOR_SET, .slot =  2,
                       .stages =  vk::ShaderStageFlagBits::eFragment,
                       .type =  vk::DescriptorType::eUniformBufferDynamic, .count =  1});

        // voxels textures
        pinfo.binding({.set =  vulkan::SHADER_DESCRIPTOR_SET, .slot =  3,
                       .stages =  vk::ShaderStageFlagBits::eFragment,
                       .type =  vk::DescriptorType::eStorageImage, .count =  1});
        pinfo.binding({.set =  vulkan::SHADER_DESCRIPTOR_SET, .slot =  4,
                       .stages =  vk::ShaderStageFlagBits::eFragment,
                       .type =  vk::DescriptorType::eStorageImage, .count =  1});
        pinfo.binding({.set =  vulkan::SHADER_DESCRIPTOR_SET, .slot =  5,
                       .stages =  vk::ShaderStageFlagBits::eFragment,
                       .type =  vk::DescriptorType::eStorageImage, .count =  1});

        r.visualization = api.create_program(std::move(pinfo));
    }

    {
        vulkan::ComputeProgramInfo pinfo{};
        pinfo.shader = api.create_shader("shaders/voxel_inject_direct_lighting.comp.spv");

        // voxel options
        pinfo.binding({.set =  vulkan::SHADER_DESCRIPTOR_SET, .slot =  0,
                       .stages =  vk::ShaderStageFlagBits::eCompute,
                       .type =  vk::DescriptorType::eUniformBufferDynamic, .count =  1});

        // directional light
        pinfo.binding({.set =  vulkan::SHADER_DESCRIPTOR_SET, .slot =  1,
                       .stages =  vk::ShaderStageFlagBits::eCompute,
                       .type =  vk::DescriptorType::eUniformBufferDynamic, .count =  1});

        // voxels textures
        // albedo
        pinfo.binding({.set =  vulkan::SHADER_DESCRIPTOR_SET, .slot =  2,
                       .stages =  vk::ShaderStageFlagBits::eCompute,
                       .type =  vk::DescriptorType::eCombinedImageSampler, .count =  1});
        // normal
        pinfo.binding({.set =  vulkan::SHADER_DESCRIPTOR_SET, .slot =  3,
                       .stages =  vk::ShaderStageFlagBits::eCompute,
                       .type =  vk::DescriptorType::eCombinedImageSampler, .count =  1});
        // radiance
        pinfo.binding({.set =  vulkan::SHADER_DESCRIPTOR_SET, .slot =  4,
                       .stages =  vk::ShaderStageFlagBits::eCompute,
                       .type =  vk::DescriptorType::eStorageImage, .count =  1});

        r.inject_radiance = api.create_program(std::move(pinfo));
    }

    {
        vulkan::ComputeProgramInfo pinfo{};
        pinfo.shader = api.create_shader("shaders/voxel_gen_aniso_base.comp.spv");
        // voxel options
        pinfo.binding({.set =  vulkan::SHADER_DESCRIPTOR_SET, .slot =  0,
                       .stages =  vk::ShaderStageFlagBits::eCompute,
                       .type =  vk::DescriptorType::eUniformBufferDynamic, .count =  1});

        // radiance
        pinfo.binding({.set =  vulkan::SHADER_DESCRIPTOR_SET, .slot =  1,
                       .stages =  vk::ShaderStageFlagBits::eCompute,
                       .type =  vk::DescriptorType::eCombinedImageSampler, .count =  1});

        // aniso volumes
        u32 count = r.voxels_directional_volumes.size();
        pinfo.binding({.set =  vulkan::SHADER_DESCRIPTOR_SET, .slot =  2,
                       .stages =  vk::ShaderStageFlagBits::eCompute,
                       .type =  vk::DescriptorType::eStorageImage, .count =  count});
        r.generate_aniso_base = api.create_program(std::move(pinfo));
    }

    {
        vulkan::ComputeProgramInfo pinfo{};
        pinfo.shader = api.create_shader("shaders/voxel_gen_aniso_mipmaps.comp.spv");

        // voxel options
        pinfo.binding({.set =  vulkan::SHADER_DESCRIPTOR_SET, .slot =  0,
                       .stages =  vk::ShaderStageFlagBits::eCompute,
                       .type =  vk::DescriptorType::eUniformBufferDynamic, .count =  1});

        // mip src
        pinfo.binding({.set =  vulkan::SHADER_DESCRIPTOR_SET, .slot =  1,
                       .stages =  vk::ShaderStageFlagBits::eCompute,
                       .type =  vk::DescriptorType::eUniformBufferDynamic, .count =  1});

        u32 count = r.voxels_directional_volumes.size();

        // radiance
        pinfo.binding({.set =  vulkan::SHADER_DESCRIPTOR_SET, .slot =  2,
                       .stages =  vk::ShaderStageFlagBits::eCompute,
                       .type =  vk::DescriptorType::eStorageImage, .count =  count});

        // aniso volumes
        pinfo.binding({.set =  vulkan::SHADER_DESCRIPTOR_SET, .slot =  3,
                       .stages =  vk::ShaderStageFlagBits::eCompute,
                       .type =  vk::DescriptorType::eStorageImage, .count =  count});
        r.generate_aniso_mipmap = api.create_program(std::move(pinfo));
    }

    /// --- Checkerboard floor
    {
        std::array<u16, 6> indices = {0, 1, 2, 0, 2, 3};

        // clang-format off
        float height = -0.001f;
        std::array vertices =
        {
            -1.0f,  height, -1.0f,      0.0f, 0.0f,
             1.0f,  height, -1.0f,      1.0f, 0.0f,
             1.0f,  height,  1.0f,      1.0f, 1.0f,
            -1.0f,  height,  1.0f,      0.0f, 1.0f,
        };
        // clang-format on

        auto &index_buffer  = r.checkerboard_floor.index_buffer;
        auto &vertex_buffer = r.checkerboard_floor.vertex_buffer;

	vulkan::BufferInfo info;
	info.name           = "Floor Index buffer";
	info.size           = indices.size() * sizeof(u16);
	info.usage          = vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst;
	info.memory_usage   = VMA_MEMORY_USAGE_GPU_ONLY;
	index_buffer        = api.create_buffer(info);

	info.name           = "Floor Vertex buffer";
	info.size           = vertices.size() * sizeof(float);
	info.usage          = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst;
	vertex_buffer       = api.create_buffer(info);

	api.upload_buffer(index_buffer,  indices.data(),  indices.size() * sizeof(u16));
	api.upload_buffer(vertex_buffer, vertices.data(), vertices.size() * sizeof(float));
    }

    {
        vulkan::GraphicsProgramInfo pinfo{};
        pinfo.vertex_shader   = api.create_shader("shaders/checkerboard_floor.vert.spv");
        pinfo.fragment_shader = api.create_shader("shaders/checkerboard_floor.frag.spv");

        // voxel options
        pinfo.binding({.set    = vulkan::GLOBAL_DESCRIPTOR_SET,
                       .slot   = 0,
                       .stages = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eCompute,
                       .type   = vk::DescriptorType::eUniformBufferDynamic,
                       .count  = 1});

        pinfo.vertex_stride(3*sizeof(float) + 2*sizeof(float));
        pinfo.vertex_info({.format = vk::Format::eR32G32B32Sfloat, .offset = 0});
        pinfo.vertex_info({.format = vk::Format::eR32G32Sfloat, .offset = 3 * sizeof(float)});

        pinfo.enable_depth_write = true;
        pinfo.depth_test = vk::CompareOp::eGreaterOrEqual;
        pinfo.depth_bias = 0.0f;

        r.checkerboard_floor.program = api.create_program(std::move(pinfo));
    }


    /// --- Sky

    {
        vulkan::ImageInfo iinfo;
        iinfo.name   = "Transmittance LUT";
        iinfo.format = vk::Format::eR16G16B16A16Sfloat;
        iinfo.width  = 256;
        iinfo.height = 64;
        iinfo.depth  = 1;
        iinfo.usages = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc
                       | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
        auto image_h = api.create_image(iinfo);

        vulkan::RTInfo cinfo;
        cinfo.is_swapchain         = false;
        cinfo.image_h              = image_h;
        r.sky.transmittance_lut_rt = api.create_rendertarget(cinfo);
    }

    {
        vulkan::GraphicsProgramInfo pinfo{};
        pinfo.vertex_shader   = api.create_shader("shaders/fullscreen_triangle.vert.spv");
        pinfo.fragment_shader = api.create_shader("shaders/transmittance_lut.frag.spv");

        pinfo.binding({.set    = vulkan::GLOBAL_DESCRIPTOR_SET,
                       .slot   = 0,
                       .stages = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute,
                       .type   = vk::DescriptorType::eUniformBufferDynamic,
                       .count  = 1});

        pinfo.binding({.set    = vulkan::SHADER_DESCRIPTOR_SET,
                       .slot   = 0,
                       .stages = vk::ShaderStageFlagBits::eFragment,
                       .type   = vk::DescriptorType::eUniformBufferDynamic,
                       .count  = 1});

        r.sky.render_transmittance = api.create_program(std::move(pinfo));
    }

    {
        vulkan::ImageInfo iinfo;
        iinfo.name   = "SkyView LUT";
        iinfo.format = vk::Format::eR16G16B16A16Sfloat;
        iinfo.width  = 192;
        iinfo.height = 108;
        iinfo.depth  = 1;
        iinfo.usages = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc
                       | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
        auto image_h = api.create_image(iinfo);

        vulkan::RTInfo cinfo;
        cinfo.is_swapchain         = false;
        cinfo.image_h              = image_h;
        r.sky.skyview_lut_rt = api.create_rendertarget(cinfo);
    }


    {
        vulkan::GraphicsProgramInfo pinfo{};
        pinfo.vertex_shader   = api.create_shader("shaders/fullscreen_triangle.vert.spv");
        pinfo.fragment_shader = api.create_shader("shaders/skyview_lut.frag.spv");

        // globla uniform
        pinfo.binding({.set    = vulkan::GLOBAL_DESCRIPTOR_SET,
                       .slot   = 0,
                       .stages = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute,
                       .type   = vk::DescriptorType::eUniformBufferDynamic,
                       .count  = 1});

        // atmosphere params
        pinfo.binding({.set    = vulkan::SHADER_DESCRIPTOR_SET,
                       .slot   = 0,
                       .stages = vk::ShaderStageFlagBits::eFragment,
                       .type   = vk::DescriptorType::eUniformBufferDynamic,
                       .count  = 1});

        // transmittance LUT
        pinfo.binding({.set    = vulkan::SHADER_DESCRIPTOR_SET,
                       .slot   = 1,
                       .stages = vk::ShaderStageFlagBits::eFragment,
                       .type   = vk::DescriptorType::eCombinedImageSampler,
                       .count  = 1});

        // multiscattering LUT
        pinfo.binding({.set    = vulkan::SHADER_DESCRIPTOR_SET,
                       .slot   = 2,
                       .stages = vk::ShaderStageFlagBits::eFragment,
                       .type   = vk::DescriptorType::eCombinedImageSampler,
                       .count  = 1});

        r.sky.render_skyview = api.create_program(std::move(pinfo));
    }

    {
        vulkan::GraphicsProgramInfo pinfo{};
        pinfo.vertex_shader   = api.create_shader("shaders/fullscreen_triangle.vert.spv");
        pinfo.fragment_shader = api.create_shader("shaders/sky_raymarch.frag.spv");

        pinfo.binding({.set    = vulkan::GLOBAL_DESCRIPTOR_SET,
                       .slot   = 0,
                       .stages = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute,
                       .type   = vk::DescriptorType::eUniformBufferDynamic,
                       .count  = 1});

        pinfo.binding({.set    = vulkan::SHADER_DESCRIPTOR_SET,
                       .slot   = 0,
                       .stages = vk::ShaderStageFlagBits::eFragment,
                       .type   = vk::DescriptorType::eUniformBufferDynamic,
                       .count  = 1});

        pinfo.binding({.set    = vulkan::SHADER_DESCRIPTOR_SET,
                       .slot   = 1,
                       .stages = vk::ShaderStageFlagBits::eFragment,
                       .type   = vk::DescriptorType::eCombinedImageSampler,
                       .count  = 1});

        pinfo.binding({.set    = vulkan::SHADER_DESCRIPTOR_SET,
                       .slot   = 2,
                       .stages = vk::ShaderStageFlagBits::eFragment,
                       .type   = vk::DescriptorType::eCombinedImageSampler,
                       .count  = 1});

        pinfo.binding({.set    = vulkan::SHADER_DESCRIPTOR_SET,
                       .slot   = 3,
                       .stages = vk::ShaderStageFlagBits::eFragment,
                       .type   = vk::DescriptorType::eCombinedImageSampler,
                       .count  = 1});

        r.sky.sky_raymarch = api.create_program(std::move(pinfo));
    }


    {
        vulkan::ImageInfo iinfo;
        iinfo.name   = "Multiscattering LUT";
        iinfo.format = vk::Format::eR16G16B16A16Sfloat;
        iinfo.width  = 32;
        iinfo.height = 32;
        iinfo.depth  = 1;
        iinfo.usages = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc
                       | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
        r.sky.multiscattering_lut = api.create_image(iinfo);
    }
    {
        vulkan::ComputeProgramInfo pinfo{};
        pinfo.shader = api.create_shader("shaders/multiscat_lut.comp.spv");
        // atmosphere params
        pinfo.binding({.slot =  0,
                       .stages =  vk::ShaderStageFlagBits::eCompute,
                       .type =  vk::DescriptorType::eUniformBufferDynamic, .count =  1});

        // transmittance lut
        pinfo.binding({.slot =  1,
                       .stages =  vk::ShaderStageFlagBits::eCompute,
                       .type =  vk::DescriptorType::eCombinedImageSampler, .count =  1});

        // multiscattering lut
        pinfo.binding({.slot =  2,
                       .stages =  vk::ShaderStageFlagBits::eCompute,
                       .type =  vk::DescriptorType::eStorageImage, .count =  1});

        r.sky.compute_multiscattering_lut = api.create_program(std::move(pinfo));
    }


    return r;
}

void Renderer::destroy()
{
    api.wait_idle();
    destroy_model();

    {
        auto &depth = api.get_rendertarget(depth_rt);
        api.destroy_image(depth.image_h);
    }
    {
        auto &color = api.get_rendertarget(color_rt);
        api.destroy_image(color.image_h);
    }

    api.destroy_image(voxels_albedo);
    api.destroy_image(voxels_normal);
    api.destroy_image(voxels_radiance);
    for (auto image_h : voxels_directional_volumes)
    {
        api.destroy_image(image_h);
    }

#if defined(ENABLE_IMGUI)
    api.destroy_image(gui_texture);
#endif

    {
        auto &image = api.get_rendertarget(sky.transmittance_lut_rt);
        api.destroy_image(image.image_h);
    }

    {
        auto &image = api.get_rendertarget(sky.skyview_lut_rt);
        api.destroy_image(image.image_h);
    }

    api.destroy();
}

void Renderer::on_resize(int width, int height)
{
    api.on_resize(width, height);

    auto &depth = api.get_rendertarget(depth_rt);
    api.destroy_image(depth.image_h);

    auto &color = api.get_rendertarget(color_rt);
    api.destroy_image(color.image_h);

    auto &lod_map = api.get_rendertarget(screenspace_lod_map_rt);
    api.destroy_image(lod_map.image_h);

    {
        vulkan::ImageInfo iinfo;
        iinfo.name   = "Depth";
        iinfo.format = vk::Format::eD32Sfloat;
        iinfo.width  = api.ctx.swapchain.extent.width;
        iinfo.height = api.ctx.swapchain.extent.height;
        iinfo.depth  = 1;
        iinfo.usages = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled;
        auto depth_h = api.create_image(iinfo);

        vulkan::RTInfo dinfo;
        dinfo.is_swapchain = false;
        dinfo.image_h      = depth_h;
        depth_rt         = api.create_rendertarget(dinfo);
    }
    {
        vulkan::ImageInfo iinfo;
        iinfo.name   = "HDR color";
        iinfo.format = vk::Format::eR16G16B16A16Sfloat;
        iinfo.width  = api.ctx.swapchain.extent.width;
        iinfo.height = api.ctx.swapchain.extent.height;
        iinfo.depth  = 1;
        iinfo.usages = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
        auto color_h = api.create_image(iinfo);

        vulkan::RTInfo cinfo;
        cinfo.is_swapchain = false;
        cinfo.image_h      = color_h;
        color_rt         = api.create_rendertarget(cinfo);
    }
    {
        vulkan::ImageInfo iinfo;
        iinfo.name   = "ShadowMap LOD map";
        iinfo.format = vk::Format::eR8Uint;
        iinfo.width  = api.ctx.swapchain.extent.width;
        iinfo.height = api.ctx.swapchain.extent.height;
        iinfo.depth  = 1;
        iinfo.usages = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
        auto lod_map_h = api.create_image(iinfo);

        vulkan::RTInfo cinfo;
        cinfo.is_swapchain     = false;
        cinfo.image_h          = lod_map_h;
        screenspace_lod_map_rt = api.create_rendertarget(cinfo);
    }
}

void Renderer::wait_idle()
{
    api.wait_idle();
}

void Renderer::reload_shader(std::string_view shader_name)
{
    std::cout << shader_name << " changed!\n";

    // Find the shader that needs to be updated
    vulkan::Shader *found = nullptr;
    for (auto &shader : api.shaders) {
        std::cerr << shader_name << " == " << shader.name << "\n";

        if (shader_name == shader.name) {
            assert(found == nullptr);
            found = &shader;
        }
    }

    if (!found) {
        assert(false);
        return;
    }

    vulkan::Shader &shader = *found;
    std::cerr << "Found " << shader.name << "\n";

    // Create a new shader module
    vulkan::ShaderH new_shader = api.create_shader(shader_name);
    std::cerr << "New shader's handle: " << new_shader.value() << "\n";

    std::vector<vulkan::ShaderH> to_remove;

    // Update programs using this shader to the new shader
    for (auto &program : api.graphics_programs) {
        if (program.info.vertex_shader.is_valid()) {
            auto &vertex_shader = api.get_shader(program.info.vertex_shader);
            if (vertex_shader.name == shader.name) {
                to_remove.push_back(program.info.vertex_shader);
                program.info.vertex_shader = new_shader;
            }
        }

        if (program.info.geom_shader.is_valid()) {
            auto &geom_shader = api.get_shader(program.info.geom_shader);
            if (geom_shader.name == shader.name) {
                to_remove.push_back(program.info.geom_shader);
                program.info.geom_shader = new_shader;
            }
        }

        if (program.info.fragment_shader.is_valid()) {
            auto &fragment_shader = api.get_shader(program.info.fragment_shader);
            if (fragment_shader.name == shader.name) {
                to_remove.push_back(program.info.fragment_shader);
                program.info.fragment_shader = new_shader;
            }
        }
    }

    for (auto &program : api.compute_programs) {
        if (program.info.shader.is_valid()) {
            auto &compute_shader = api.get_shader(program.info.shader);
            if (compute_shader.name == shader.name) {
                to_remove.push_back(program.info.shader);
                program.info.shader = new_shader;
            }
        }
    }

    assert(!to_remove.empty());

    // Destroy the old shaders
    for (vulkan::ShaderH shader_h : to_remove) {
        std::cerr << "Removing handle: " << shader_h.value() << "\n";
        api.destroy_shader(shader_h);
    }
}

void Renderer::imgui_draw()
{

#if defined(ENABLE_IMGUI)
    ImGui::Render();
    ImDrawData *data = ImGui::GetDrawData();
    if (data == nullptr || data->TotalVtxCount == 0) {
        return;
    }
    api.begin_label("ImGui");

    u32 vbuffer_len = sizeof(ImDrawVert) * static_cast<u32>(data->TotalVtxCount);
    u32 ibuffer_len = sizeof(ImDrawIdx) * static_cast<u32>(data->TotalIdxCount);

    auto v_pos = api.dynamic_vertex_buffer(vbuffer_len);
    auto i_pos = api.dynamic_index_buffer(ibuffer_len);

    auto *vertices = reinterpret_cast<ImDrawVert *>(v_pos.mapped);
    auto *indices  = reinterpret_cast<ImDrawIdx *>(i_pos.mapped);

    for (int i = 0; i < data->CmdListsCount; i++) {
        const ImDrawList *cmd_list = data->CmdLists[i];

        std::memcpy(vertices, cmd_list->VtxBuffer.Data, sizeof(ImDrawVert) * size_t(cmd_list->VtxBuffer.Size));
        std::memcpy(indices, cmd_list->IdxBuffer.Data, sizeof(ImDrawIdx) * size_t(cmd_list->IdxBuffer.Size));

        vertices += cmd_list->VtxBuffer.Size;
        indices += cmd_list->IdxBuffer.Size;
    }

    vk::Viewport viewport{};
    viewport.width    = data->DisplaySize.x * data->FramebufferScale.x;
    viewport.height   = data->DisplaySize.y * data->FramebufferScale.y;
    viewport.minDepth = 1.0f;
    viewport.maxDepth = 1.0f;
    api.set_viewport(viewport);

    api.bind_vertex_buffer(v_pos);
    api.bind_index_buffer(i_pos);

    float4 scale_and_translation;
    scale_and_translation[0] = 2.0f / data->DisplaySize.x;                            // X Scale
    scale_and_translation[1] = 2.0f / data->DisplaySize.y;                            // Y Scale
    scale_and_translation[2] = -1.0f - data->DisplayPos.x * scale_and_translation[0]; // X Translation
    scale_and_translation[3] = -1.0f - data->DisplayPos.y * scale_and_translation[1]; // Y Translation

    // Will project scissor/clipping rectangles into framebuffer space
    ImVec2 clip_off   = data->DisplayPos;       // (0,0) unless using multi-viewports
    ImVec2 clip_scale = data->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

    // check the layout of images to render
    for (int list = 0; list < data->CmdListsCount; list++) {
        const ImDrawList *cmd_list = data->CmdLists[list];

        for (int command_index = 0; command_index < cmd_list->CmdBuffer.Size; command_index++) {
            const ImDrawCmd *draw_command = &cmd_list->CmdBuffer[command_index];

            if (draw_command->TextureId) {
                auto image_h = vulkan::ImageH(static_cast<u32>(reinterpret_cast<u64>(draw_command->TextureId)));
                auto &image = api.get_image(image_h);

                auto next_access = THSVS_ACCESS_ANY_SHADER_READ_SAMPLED_IMAGE_OR_UNIFORM_TEXEL_BUFFER;
                auto next_layout = vk::ImageLayout::eShaderReadOnlyOptimal;

                if (image.image_info.usage & vk::ImageUsageFlagBits::eDepthStencilAttachment) {
                    next_access = THSVS_ACCESS_FRAGMENT_SHADER_READ_DEPTH_STENCIL_INPUT_ATTACHMENT;
                    next_layout = vk::ImageLayout::eDepthReadOnlyOptimal;
                }

                transition_if_needed_internal(api, image, next_access, next_layout);
            }
        }
    }

    vulkan::PassInfo pass;
    pass.present = true;

    vulkan::AttachmentInfo color_info;
    color_info.load_op = vk::AttachmentLoadOp::eLoad;
    color_info.rt      = swapchain_rt;
    pass.color         = std::make_optional(color_info);

    api.begin_pass(std::move(pass));

    enum UIProgram {
        Float,
        Uint
    };

    // Render GUI
    i32 vertex_offset = 0;
    u32 index_offset  = 0;
    for (int list = 0; list < data->CmdListsCount; list++) {
        const ImDrawList *cmd_list = data->CmdLists[list];

        for (int command_index = 0; command_index < cmd_list->CmdBuffer.Size; command_index++) {
            const ImDrawCmd *draw_command = &cmd_list->CmdBuffer[command_index];

            vulkan::GraphicsProgramH current = gui_program;

            if (draw_command->TextureId) {
                auto texture = vulkan::ImageH(static_cast<u32>(reinterpret_cast<u64>(draw_command->TextureId)));
                auto& image = api.get_image(texture);

                if (image.image_info.format == vk::Format::eR32Uint)
                {
                    current = gui_uint_program;
                }

                api.bind_image(current, vulkan::SHADER_DESCRIPTOR_SET, 0, texture);
            }
            else {
                api.bind_image(current, vulkan::SHADER_DESCRIPTOR_SET, 0, gui_texture);
            }

            api.bind_program(current);
            api.push_constant(vk::ShaderStageFlagBits::eVertex, 0, sizeof(float4), &scale_and_translation);

            // Project scissor/clipping rectangles into framebuffer space
            ImVec4 clip_rect;
            clip_rect.x = (draw_command->ClipRect.x - clip_off.x) * clip_scale.x;
            clip_rect.y = (draw_command->ClipRect.y - clip_off.y) * clip_scale.y;
            clip_rect.z = (draw_command->ClipRect.z - clip_off.x) * clip_scale.x;
            clip_rect.w = (draw_command->ClipRect.w - clip_off.y) * clip_scale.y;

            // Apply scissor/clipping rectangle
            // FIXME: We could clamp width/height based on clamped min/max values.
            vk::Rect2D scissor;
            scissor.offset.x      = (static_cast<i32>(clip_rect.x) > 0) ? static_cast<i32>(clip_rect.x) : 0;
            scissor.offset.y      = (static_cast<i32>(clip_rect.y) > 0) ? static_cast<i32>(clip_rect.y) : 0;
            scissor.extent.width  = static_cast<u32>(clip_rect.z - clip_rect.x);
            scissor.extent.height = static_cast<u32>(clip_rect.w - clip_rect.y + 1); // FIXME: Why +1 here?

            api.set_scissor(scissor);

            api.draw_indexed(draw_command->ElemCount, 1, index_offset, vertex_offset, 0);

            index_offset += draw_command->ElemCount;
        }
        vertex_offset += cmd_list->VtxBuffer.Size;
    }
    api.end_pass();
    api.end_label();
#endif
}

static void bind_texture(Renderer &r, vulkan::GraphicsProgramH program_h, uint slot, std::optional<u32> i_texture)
{
    auto &api = r.api;
    if (i_texture) {
        auto &texture = r.model.textures[*i_texture];
        auto &image   = r.model.images[texture.image];
        auto &sampler = r.model.samplers[texture.sampler];

        api.bind_combined_image_sampler(program_h, vulkan::DRAW_DESCRIPTOR_SET, slot, image.image_h, sampler.sampler_h);
    }
    else {
        // bind empty texture
    }
}

static void draw_node(Renderer &r, Node &node)
{
    auto &api = r.api;
    if (node.dirty) {
        node.dirty            = false;
        auto translation      = glm::translate(glm::mat4(1.0f), node.translation);
        auto rotation         = glm::mat4(node.rotation);
        auto scale            = glm::scale(glm::mat4(1.0f), node.scale);
        node.cached_transform = translation * rotation * scale;
    }

    auto u_pos   = api.dynamic_uniform_buffer(sizeof(float4x4));
    auto *buffer = reinterpret_cast<float4x4 *>(u_pos.mapped);
    *buffer      = node.cached_transform;
    api.bind_buffer(r.model.program, vulkan::DRAW_DESCRIPTOR_SET, 0, u_pos);

    const auto &mesh = r.model.meshes[node.mesh];
    for (const auto &primitive : mesh.primitives)
    {
        // if program != last program then bind program

        const auto &material = r.model.materials[primitive.material];

        MaterialPushConstant material_pc = MaterialPushConstant::from(material);
        api.push_constant(vk::ShaderStageFlagBits::eFragment, 0, sizeof(material_pc), &material_pc);

        bind_texture(r, r.model.program, 1, material.base_color_texture);
        bind_texture(r, r.model.program, 2, material.normal_texture);
        bind_texture(r, r.model.program, 3, material.metallic_roughness_texture);

        api.draw_indexed(primitive.index_count, 1, primitive.first_index, static_cast<i32>(primitive.first_vertex), 0);
    }

    // TODO: transform relative to parent
    for (auto child_i : node.children) {
        draw_node(r, r.model.nodes[child_i]);
    }
}

void draw_floor(Renderer &r)
{
    auto &api = r.api;
    api.begin_label("Draw floor");

    api.set_viewport_and_scissor(api.ctx.swapchain.extent.width, api.ctx.swapchain.extent.height);

    // Bind camera uniform buffer
    api.bind_buffer(r.checkerboard_floor.program, vulkan::GLOBAL_DESCRIPTOR_SET, 0, r.global_uniform_pos);

    vulkan::PassInfo pass;
    pass.present = false;

    vulkan::AttachmentInfo color_info;
    color_info.load_op = vk::AttachmentLoadOp::eClear;
    color_info.rt      = r.color_rt;
    pass.color         = std::make_optional(color_info);

    vulkan::AttachmentInfo depth_info;
    depth_info.load_op = vk::AttachmentLoadOp::eLoad;
    depth_info.rt      = r.depth_rt;
    pass.depth         = std::make_optional(depth_info);

    api.begin_pass(std::move(pass));

    api.bind_program(r.checkerboard_floor.program);
    api.bind_index_buffer(r.checkerboard_floor.index_buffer);
    api.bind_vertex_buffer(r.checkerboard_floor.vertex_buffer);

    api.draw_indexed(6, 1, 0, 0, 0);

    api.end_pass();
    api.end_label();
}

void Renderer::draw_model()
{
    static usize s_selected = 0;
    static float s_opacity = 1.0f;
    static float s_trace_dist = 2.0f;
    static float s_occlusion = 1.0f;
    static float s_sampling_factor = 1.0f;
    static float s_start = 1.0f;

#if defined(ENABLE_IMGUI)
    if (p_ui->begin_window("glTF Shader"))
    {
        ImGui::SliderFloat("Output opacity", &s_opacity, 0.0f, 1.0f);
        static std::array options{"Nothing", "BaseColor", "Normals", "AO", "Indirect lighting"};
        tools::imgui_select("Debug output", options.data(), options.size(), s_selected);
        ImGui::SliderFloat("Trace dist.", &s_trace_dist, 0.0f, 1.0f);
        ImGui::SliderFloat("Occlusion factor", &s_occlusion, 0.0f, 1.0f);
        ImGui::SliderFloat("Sampling factor", &s_sampling_factor, 0.1f, 2.0f);
        ImGui::SliderFloat("Start position", &s_start, 0.1f, 2.0f);
        p_ui->end_window();
    }
#endif
    if (s_opacity == 0.0f) {
        return;
    }

    api.begin_label("Draw glTF model");

    api.set_viewport_and_scissor(api.ctx.swapchain.extent.width, api.ctx.swapchain.extent.height);


    // Bind camera uniform buffer
    api.bind_buffer(model.program, vulkan::GLOBAL_DESCRIPTOR_SET, 0, global_uniform_pos);

    // Make a shader debugging window and its own uniform buffer
    {
        auto u_pos   = api.dynamic_uniform_buffer(sizeof(ShaderDebug) + 4 * sizeof(float));
        auto *buffer = reinterpret_cast<ShaderDebug *>(u_pos.mapped);
        buffer->selected = static_cast<uint>(s_selected);
        buffer->opacity  = s_opacity;
        auto *floatbuffer = reinterpret_cast<float*>(buffer + 1);
        floatbuffer[0] = s_trace_dist;
        floatbuffer[1] = s_occlusion;
        floatbuffer[2] = s_sampling_factor;
        floatbuffer[3] = s_start;
        api.bind_buffer(model.program, vulkan::SHADER_DESCRIPTOR_SET, 0, u_pos);
    }

    // voxel options
    {
        auto u_pos     = api.dynamic_uniform_buffer(sizeof(VoxelDebug));
        auto *buffer   = reinterpret_cast<VoxelDebug *>(u_pos.mapped);
        *buffer = voxel_options;

        api.bind_buffer(model.program, vulkan::SHADER_DESCRIPTOR_SET, 1, u_pos);
    }

    // voxel textures
    {
        {
            auto &image = api.get_image(voxels_radiance);
            transition_if_needed_internal(api, image, THSVS_ACCESS_GENERAL, vk::ImageLayout::eGeneral);
        }

        api.bind_combined_image_sampler(model.program,
                                        vulkan::SHADER_DESCRIPTOR_SET,
                                        2,
                                        voxels_radiance,
                                        trilinear_sampler);

        {
            std::vector<vk::ImageView> views;
            views.reserve(voxels_directional_volumes.size());
            for (const auto& volume_h : voxels_directional_volumes)
            {
                auto &image = api.get_image(volume_h);
                transition_if_needed_internal(api, image, THSVS_ACCESS_ANY_SHADER_READ_SAMPLED_IMAGE_OR_UNIFORM_TEXEL_BUFFER, vk::ImageLayout::eShaderReadOnlyOptimal);
                views.push_back(api.get_image(volume_h).default_view);
            }
            api.bind_combined_images_sampler(model.program,
                                             vulkan::SHADER_DESCRIPTOR_SET,
                                             3,
                                             voxels_directional_volumes,
                                             trilinear_sampler,
                                             views);
        }
    }

    vulkan::PassInfo pass;
    pass.present = false;

    vulkan::AttachmentInfo color_info;
    color_info.load_op = vk::AttachmentLoadOp::eLoad;
    color_info.rt      = color_rt;
    pass.color         = std::make_optional(color_info);

    vulkan::AttachmentInfo depth_info;
    depth_info.load_op = vk::AttachmentLoadOp::eLoad;
    depth_info.rt      = depth_rt;
    pass.depth         = std::make_optional(depth_info);

    api.begin_pass(std::move(pass));

    api.bind_program(model.program);
    api.bind_index_buffer(model.index_buffer);
    api.bind_vertex_buffer(model.vertex_buffer);

    for (usize node_i : model.scene) {
        draw_node(*this, model.nodes[node_i]);
    }

    api.end_pass();
    api.end_label();
}

static void draw_node_shadow(Renderer &r, Node &node)
{
    auto &api = r.api;
    if (node.dirty) {
        node.dirty            = false;
        auto translation      = glm::translate(glm::mat4(1.0f), node.translation);
        auto rotation         = glm::mat4(node.rotation);
        auto scale            = glm::scale(glm::mat4(1.0f), node.scale);
        node.cached_transform = translation * rotation * scale;
    }

    auto u_pos   = api.dynamic_uniform_buffer(sizeof(float4x4));
    auto *buffer = reinterpret_cast<float4x4 *>(u_pos.mapped);
    *buffer      = node.cached_transform;
    api.bind_buffer(r.model_prepass, vulkan::DRAW_DESCRIPTOR_SET, 0, u_pos);

    const auto &mesh = r.model.meshes[node.mesh];
    for (const auto &primitive : mesh.primitives) {
        const auto &material = r.model.materials[primitive.material];
        bind_texture(r, r.model_prepass, 1, material.base_color_texture);
        api.draw_indexed(primitive.index_count, 1, primitive.first_index, static_cast<i32>(primitive.first_vertex), 0);
    }

    // TODO: transform relative to parent
    for (auto child_i : node.children) {
        draw_node(r, r.model.nodes[child_i]);
    }
}

static void prepass(Renderer &r)
{
    auto &api = r.api;
    api.begin_label("Prepass");

    /// --- First fill the depth buffer and write the desired lod of each pixel into a screenspace lod map
    {
    vulkan::PassInfo pass;
    pass.present = false;

    vulkan::AttachmentInfo depth_info;
    depth_info.load_op = vk::AttachmentLoadOp::eClear;
    depth_info.rt      = r.depth_rt;
    pass.depth         = std::make_optional(depth_info);

    vulkan::AttachmentInfo lod_map_info;
    lod_map_info.load_op = vk::AttachmentLoadOp::eClear;
    lod_map_info.rt      = r.screenspace_lod_map_rt;
    pass.color           = std::make_optional(lod_map_info);

    api.begin_pass(std::move(pass));

    auto depth_rt  = api.get_rendertarget(r.depth_rt);
    auto depth_img = api.get_image(depth_rt.image_h);

    api.set_viewport_and_scissor(depth_img.info.width, depth_img.info.height);

    api.bind_buffer(r.model_prepass, vulkan::GLOBAL_DESCRIPTOR_SET, 0, r.global_uniform_pos);

    api.bind_program(r.model_prepass);
    api.bind_index_buffer(r.model.index_buffer);
    api.bind_vertex_buffer(r.model.vertex_buffer);

    for (usize node_i : r.model.scene) {
        draw_node_shadow(r, r.model.nodes[node_i]);
    }

    api.end_pass();
    }

    /// --- Then build the min lod map, a texture where each texel map to 1 tile of the sparse shadow map and contains the min lod of the tile

    uint frame_idx = api.ctx.frame_count % vulkan::FRAMES_IN_FLIGHT;
    vulkan::ImageH min_lod_map = r.min_lod_map_per_frame[frame_idx];
    auto &min_lod_map_img = api.get_image(min_lod_map);

    {
        vk::ClearColorValue clear{};
        clear.uint32[0] = 99; // need high value because we are doing min operations on it
        api.clear_image(min_lod_map, clear);

        auto &program = r.fill_min_lod_map;

        api.bind_buffer(program, 0, r.global_uniform_pos);

        {
            auto &rt = api.get_rendertarget(r.screenspace_lod_map_rt);
            auto &image = api.get_image(rt.image_h);
            transition_if_needed_internal(api,
                                          image,
                                          THSVS_ACCESS_ANY_SHADER_READ_SAMPLED_IMAGE_OR_UNIFORM_TEXEL_BUFFER,
                                          vk::ImageLayout::eShaderReadOnlyOptimal);
            api.bind_combined_image_sampler(program, 1, rt.image_h, r.nearest_sampler);
        }

        {
            auto &rt = api.get_rendertarget(r.depth_rt);
            auto &image = api.get_image(rt.image_h);
            transition_if_needed_internal(api,
                                          image,
                                          THSVS_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ,
                                          vk::ImageLayout::eDepthStencilReadOnlyOptimal);
            api.bind_combined_image_sampler(program, 2, rt.image_h, r.nearest_sampler);
        }

        {
            transition_if_needed_internal(api, min_lod_map_img, THSVS_ACCESS_GENERAL, vk::ImageLayout::eGeneral);
            api.bind_image(program, 3, min_lod_map);
        }

        auto size_x = api.ctx.swapchain.extent.width / 8;
        auto size_y = api.ctx.swapchain.extent.height / 8;
        api.dispatch(program, size_x, size_y, 1);

        transition_if_needed_internal(api, min_lod_map_img, THSVS_ACCESS_HOST_READ, vk::ImageLayout::eGeneral);
    }

#if defined(ENABLE_IMGUI)
    {
        if (r.p_ui->begin_window("Sparse Shadow Map"))
        {
            ImGui::Text("Min lod map:");
            ImGui::Image(reinterpret_cast<void*>(min_lod_map.value()), ImVec2(256, 256));
            r.p_ui->end_window();
        }
    }
#endif

    /// --- Readback min lod map and remap pages


    auto &ctx = api.ctx;
    auto &frame_resource = ctx.frame_resources.get_current();
    auto &cmd            = frame_resource.command_buffer;
    auto graphics_queue   = ctx.device->getQueue(ctx.graphics_family_idx, 0);

    // wait for gpu to read min lod map
    if (0)
    {
        vk::UniqueFence fence = ctx.device->createFenceUnique({});

        cmd->end();

        vk::SubmitInfo si{};
        si.commandBufferCount = 1;
        si.pCommandBuffers    = &cmd.get();
        graphics_queue.submit(si, *fence);

        ctx.device->waitForFences({*fence}, VK_FALSE, UINT64_MAX);
    }

    constexpr usize MAX_REQUESTS = 1024;

    std::vector<TileAllocationRequest> requests;
    requests.reserve(MAX_REQUESTS);

    {
        auto ptr = api.read_image(min_lod_map);
        auto *lods = reinterpret_cast<u32*>(ptr.data);

        usize mip_size = 128; // width (and height because it's squared) of the i_mip mip level of the shadow map
        usize mip_tile_size = 128; // size of the i_mip mip level in the min lod map (mip 7 covers the entire map and mip 0 is 1 texel)
        requests.push_back({.offset = {}, .mip_level = 7});

        for (int i_mip = 6; i_mip >= 0; i_mip--)
        {
            mip_size *= 2; // each mip get more pixels (coarse -> fine)

            assert(mip_tile_size > 1);
            mip_tile_size /= 2; // because each mip gets finer the size each mip covers in the min lod map gets smaller

            const uint step = 128;

            // traverse each tiles for this mip level
            for (u32 y = 0; y < mip_size; y += step)
            {
                for (u32 x = 0; x < mip_size; x += step)
                {
                    auto offset = vk::Offset3D{static_cast<i32>(x), static_cast<i32>(y), 0};

                    for (uint row = offset.y; row < y + mip_tile_size; row++)
                    {
                        for (uint col = offset.x; col < x + mip_tile_size; col++)
                        {
                            u32 lod = lods[row * 128 + col];
                            if (lod == 99) {
                                continue;
                            }

                            if (lod >= static_cast<uint>(i_mip)) {
                                requests.push_back({.offset = offset, .mip_level = static_cast<uint>(i_mip)});

                                if (requests.size() >= MAX_REQUESTS) {
                                    goto allocations_done;
                                }

                                goto tile_search_end;
                            }
                        }
                    }
tile_search_end:
                    (void)(0); // statement needed for label?
                }
            }
        }
allocations_done:
        (void)(0); // statement needed for label?
    }

#if defined(ENABLE_SPARSE)
    {
        auto &shadow_map_rt = api.get_rendertarget(r.shadow_map_rt);
        auto &shadow_map = api.get_image(shadow_map_rt.image_h);

        const auto page_count =  shadow_map.sparse_allocations.size();
        // const auto page_size  = shadow_map.page_size;

        // sort lods and allocate page (this better be really fast)
        std::vector<vk::SparseImageMemoryBind> binds;
        binds.resize(page_count);

        assert(requests.size() == page_count);

        for (uint i = 0; i < page_count; ++i)
        {
            binds[i]                        = vk::SparseImageMemoryBind{};
            binds[i].flags                  = {};
            binds[i].subresource.arrayLayer = 0;
            binds[i].subresource.aspectMask = vk::ImageAspectFlagBits::eColor;
            binds[i].subresource.mipLevel   = requests[i].mip_level;
            binds[i].offset                 = requests[i].offset;
            binds[i].extent                 = vk::Extent3D{128, 128, 1};
            binds[i].memory                 = shadow_map.allocations_infos[i].deviceMemory;
            binds[i].memoryOffset           = shadow_map.allocations_infos[i].offset;
        }

        vk::SparseImageMemoryBindInfo img{};
        img.image     = shadow_map.vkhandle;
        img.pBinds    = binds.data();
        img.bindCount = binds.size();

        vk::BindSparseInfo info{};
        info.pImageBinds    = &img;
        info.imageBindCount = 1;
        graphics_queue.bindSparse(info, {});
    }
#endif

    if (0)
    {
    // restart command buffer
    vk::CommandBufferBeginInfo binfo{};
    cmd->begin(binfo);
    }

    /// --- Render shadow map

    if (0)
    {
        vulkan::PassInfo pass;
        pass.present = false;

        vulkan::AttachmentInfo shadowmap_info;
        shadowmap_info.load_op = vk::AttachmentLoadOp::eClear;
        shadowmap_info.rt      = r.shadow_map_rt;
        pass.color             = std::make_optional(shadowmap_info);

        api.begin_pass(std::move(pass));

        auto depth_rt  = api.get_rendertarget(r.depth_rt);
        auto depth_img = api.get_image(depth_rt.image_h);

        api.set_viewport_and_scissor(depth_img.info.width, depth_img.info.height);

        api.bind_buffer(r.model_prepass, vulkan::GLOBAL_DESCRIPTOR_SET, 0, r.global_uniform_pos);

        api.bind_program(r.model_prepass);
        api.bind_index_buffer(r.model.index_buffer);
        api.bind_vertex_buffer(r.model.vertex_buffer);

        for (usize node_i : r.model.scene) {
            draw_node_shadow(r, r.model.nodes[node_i]);
        }

        api.end_pass();
    }
    api.end_label();
}


static void voxelize_node(Renderer &r, Node &node)
{
    auto &api = r.api;
    if (node.dirty) {
        node.dirty            = false;
        auto translation      = glm::translate(glm::mat4(1.0f), node.translation);
        auto rotation         = glm::mat4(node.rotation);
        auto scale            = glm::scale(glm::mat4(1.0f), node.scale);
        node.cached_transform = translation * rotation * scale;
    }

    auto u_pos   = api.dynamic_uniform_buffer(sizeof(float4x4));
    auto *buffer = reinterpret_cast<float4x4 *>(u_pos.mapped);
    *buffer      = node.cached_transform;
    api.bind_buffer(r.voxelization, vulkan::DRAW_DESCRIPTOR_SET, 0, u_pos);

    const auto &mesh = r.model.meshes[node.mesh];
    for (const auto &primitive : mesh.primitives) {
        // if program != last program then bind program

        const auto &material = r.model.materials[primitive.material];

        bind_texture(r, r.voxelization, 1, material.base_color_texture);
        bind_texture(r, r.voxelization, 2, material.normal_texture);

        api.draw_indexed(primitive.index_count, 1, primitive.first_index, static_cast<i32>(primitive.first_vertex), 0);
    }

    // TODO: transform relative to parent
    for (auto child_i : node.children) {
        draw_node(r, r.model.nodes[child_i]);
    }
}


void Renderer::voxelize_scene()
{
    api.begin_label("Voxelization");
    api.set_viewport_and_scissor(voxel_options.res, voxel_options.res);

    // Bind voxel debug
    {
#if defined(ENABLE_IMGUI)
        if (p_ui->begin_window("Voxelization"))
        {
            ImGui::SliderFloat3("Center", &voxel_options.center[0], -40.f, 40.f);
            voxel_options.center = glm::floor(voxel_options.center);
            ImGui::SliderFloat("Voxel size (m)", &voxel_options.size, 0.01f, 0.1f);
            p_ui->end_window();
        }
#endif
        auto u_pos     = api.dynamic_uniform_buffer(sizeof(VoxelDebug));
        auto *buffer   = reinterpret_cast<VoxelDebug *>(u_pos.mapped);
        *buffer = voxel_options;

        api.bind_buffer(voxelization, vulkan::SHADER_DESCRIPTOR_SET, 0, u_pos);
    }

    // Bind projection cameras
    {
        auto u_pos     = api.dynamic_uniform_buffer(3 * sizeof(float4x4));
        auto *buffer   = reinterpret_cast<float4x4 *>(u_pos.mapped);

        float res = voxel_options.res;
        res *= voxel_options.size;
        float halfsize = res / 2;

        auto center = voxel_options.center + float3(halfsize);

        auto projection = glm::ortho(-halfsize, halfsize, -halfsize, halfsize, 0.0f, res);
        buffer[0] = projection * glm::lookAt(center + float3(halfsize, 0.f, 0.f), center, float3(0.f, 1.f, 0.f));
        buffer[1] = projection * glm::lookAt(center + float3(0.f, halfsize, 0.f), center, float3(0.f, 0.f, -1.f));
        buffer[2] = projection * glm::lookAt(center + float3(0.f, 0.f, halfsize), center, float3(0.f, 1.f, 0.f));

        api.bind_buffer(voxelization, vulkan::SHADER_DESCRIPTOR_SET, 1, u_pos);
    }

    // Bind voxel textures

    // use the default format
    auto &albedo_uint = api.get_image(voxels_albedo).format_views[0];
    auto &normal_uint = api.get_image(voxels_normal).format_views[0];

    {
    auto &image = api.get_image(voxels_albedo);
    transition_if_needed_internal(api, image, THSVS_ACCESS_GENERAL, vk::ImageLayout::eGeneral);
    }
    api.bind_image(voxelization, vulkan::SHADER_DESCRIPTOR_SET, 2, voxels_albedo, albedo_uint);

    {
    auto &image = api.get_image(voxels_normal);
    transition_if_needed_internal(api, image, THSVS_ACCESS_GENERAL, vk::ImageLayout::eGeneral);
    }
    api.bind_image(voxelization, vulkan::SHADER_DESCRIPTOR_SET, 3, voxels_normal, normal_uint);


    vulkan::PassInfo pass{};
    pass.samples = vk::SampleCountFlagBits::e16;
    api.begin_pass(std::move(pass));

    api.bind_program(voxelization);
    api.bind_index_buffer(model.index_buffer);
    api.bind_vertex_buffer(model.vertex_buffer);

    for (usize node_i : model.scene) {
        voxelize_node(*this, model.nodes[node_i]);
    }

    api.end_pass();
    api.end_label();
}

void Renderer::visualize_voxels()
{
    static usize s_selected = 3;
    static float s_opacity = 0.0f;
#if defined(ENABLE_IMGUI)
    if (p_ui->begin_window("Voxels Shader"))
    {
        ImGui::SliderFloat("Output opacity", &s_opacity, 0.0f, 1.0f);
        static std::array options{"Nothing", "Albedo", "Normal", "Radiance"};
        tools::imgui_select("Debug output", options.data(), options.size(), s_selected);
        p_ui->end_window();
    }
#endif
    if (s_opacity == 0.0f || s_selected == 0) {
        return;
    }

    api.begin_label("Voxel visualization");

    api.set_viewport_and_scissor(api.ctx.swapchain.extent.width, api.ctx.swapchain.extent.height);

    // Bind voxel options
    {
        auto u_pos     = api.dynamic_uniform_buffer(sizeof(VoxelDebug));
        auto *buffer   = reinterpret_cast<VoxelDebug *>(u_pos.mapped);
        *buffer = voxel_options;

        api.bind_buffer(visualization, vulkan::SHADER_DESCRIPTOR_SET, 0, u_pos);
    }

    // Bind camera uniform buffer
    {
        auto u_pos   = api.dynamic_uniform_buffer(3 * sizeof(float3) + sizeof(float));
        auto *buffer = reinterpret_cast<float4 *>(u_pos.mapped);
        buffer[0]    = float4(p_camera->position, 0.0f);
        buffer[1]    = float4(p_camera->front, 0.0f);
        buffer[2]    = float4(p_camera->up, 0.0f);

        api.bind_buffer(visualization, vulkan::SHADER_DESCRIPTOR_SET, 1, u_pos);
    }

    // Bind debug options
    {
        auto u_pos   = api.dynamic_uniform_buffer(sizeof(ShaderDebug));
        auto *buffer = reinterpret_cast<ShaderDebug *>(u_pos.mapped);
        buffer->selected = static_cast<uint>(s_selected);
        buffer->opacity  = s_opacity;
        api.bind_buffer(visualization, vulkan::SHADER_DESCRIPTOR_SET, 2, u_pos);
    }

    auto &image = api.get_image(voxels_albedo);
    transition_if_needed_internal(api, image, THSVS_ACCESS_GENERAL, vk::ImageLayout::eGeneral);
    api.bind_image(visualization, vulkan::SHADER_DESCRIPTOR_SET, 3, voxels_albedo);
    {
    auto &image = api.get_image(voxels_normal);
    transition_if_needed_internal(api, image, THSVS_ACCESS_GENERAL, vk::ImageLayout::eGeneral);
    }
    api.bind_image(visualization, vulkan::SHADER_DESCRIPTOR_SET, 4, voxels_normal);
    {
    auto &image = api.get_image(voxels_radiance);
    transition_if_needed_internal(api, image, THSVS_ACCESS_GENERAL, vk::ImageLayout::eGeneral);
    }
    api.bind_image(visualization, vulkan::SHADER_DESCRIPTOR_SET, 5, voxels_radiance);


    vulkan::PassInfo pass;
    pass.present = false;

    vulkan::AttachmentInfo color_info;
    color_info.load_op = vk::AttachmentLoadOp::eClear;
    color_info.rt      = color_rt;
    pass.color         = std::make_optional(color_info);

    api.begin_pass(std::move(pass));

    api.bind_program(visualization);

    api.draw(3, 1, 0, 0);

    api.end_pass();
    api.end_label();
}


struct DirectLightingDebug
{
    float4 sun_direction;
    float4 point_position;
    float point_scale;
    float trace_shadow_hit;
    float max_dist;
    float first_step;
};

void Renderer::inject_direct_lighting()
{
    static std::array s_position       = {1.5f, 2.5f, 0.0f};
    static std::array s_sun_direction       = {8.f, -90.f, 0.f};
    static float s_scale            = 1.0f;
    static float s_trace_shadow_hit = 0.5f;
    static auto s_max_dist          = static_cast<float>(voxel_options.res);
    static float s_first_step          = 2.0f;
#if defined(ENABLE_IMGUI)
    if (p_ui->begin_window("Voxels Direct Lighting"))
    {
        if (ImGui::Button("Reload shader"))
        {
            reload_shader("shaders/voxel_inject_direct_lighting.comp.spv");
        }
        ImGui::SliderFloat3("Point light position", &s_position[0], -10.0f, 10.0f);
        ImGui::SliderFloat("Point light scale", &s_scale, 0.1f, 10.f);
        ImGui::SliderFloat3("Sun rotation", s_sun_direction.data(), -180.0f, 180.0f);
        ImGui::SliderFloat3("Sun front", &sun.front[0], -180.0f, 180.0f);
        ImGui::SliderFloat("Trace Shadow Hit", &s_trace_shadow_hit, 0.0f, 1.0f);
        ImGui::SliderFloat("Max Dist", &s_max_dist, 0.0f, 300.0f);
        ImGui::SliderFloat("First step", &s_first_step, 1.0f, 20.0f);
        p_ui->end_window();
    }
#endif

    api.begin_label("Inject direct lighting");

    auto &program = inject_radiance;

    // Bind voxel options
    {
        auto u_pos     = api.dynamic_uniform_buffer(sizeof(VoxelDebug));
        auto *buffer   = reinterpret_cast<VoxelDebug *>(u_pos.mapped);
        *buffer = voxel_options;

        api.bind_buffer(program, 0, u_pos);
    }

    // Bind camera uniform buffer
    {
        auto u_pos   = api.dynamic_uniform_buffer(sizeof(DirectLightingDebug));
        auto *buffer = reinterpret_cast<DirectLightingDebug *>(u_pos.mapped);

        sun.pitch = s_sun_direction[0];
        sun.yaw  = s_sun_direction[1];
        sun.roll = s_sun_direction[2];
        sun.update_view();
        buffer->sun_direction    = float4(sun.front, 1);
        buffer->point_position   = float4(s_position[0], s_position[1], s_position[2], 1);
        buffer->point_scale      = s_scale;
        buffer->trace_shadow_hit = s_trace_shadow_hit;
        buffer->max_dist         = s_max_dist;
        buffer->first_step       = s_first_step;
        api.bind_buffer(program, 1, u_pos);
    }

    // use the RGBA8 format defined at creation in view_formats
    {
    auto &image = api.get_image(voxels_albedo);
    transition_if_needed_internal(api, image, THSVS_ACCESS_ANY_SHADER_READ_SAMPLED_IMAGE_OR_UNIFORM_TEXEL_BUFFER, vk::ImageLayout::eShaderReadOnlyOptimal);
    }
    api.bind_combined_image_sampler(program, 2, voxels_albedo, trilinear_sampler);

    {
    auto &image = api.get_image(voxels_normal);
    transition_if_needed_internal(api, image, THSVS_ACCESS_ANY_SHADER_READ_SAMPLED_IMAGE_OR_UNIFORM_TEXEL_BUFFER, vk::ImageLayout::eShaderReadOnlyOptimal);
    api.bind_combined_image_sampler(program, 3, voxels_normal, trilinear_sampler);
    }

    {
    auto &image = api.get_image(voxels_radiance);
    transition_if_needed_internal(api, image, THSVS_ACCESS_GENERAL, vk::ImageLayout::eGeneral);
    }
    api.bind_image(program, 4, voxels_radiance);


    auto count = voxel_options.res / 8;
    api.dispatch(program, count, count, count);
    api.end_label();
}

void Renderer::generate_aniso_voxels()
{
    api.begin_label("Compute anisotropic voxels");

    auto &cmd = *api.ctx.frame_resources.get_current().command_buffer;

    auto voxel_size = voxel_options.size * 2;
    auto voxel_res = voxel_options.res / 2;

    // Bind voxel options
    {
        auto u_pos     = api.dynamic_uniform_buffer(sizeof(VoxelDebug));
        auto *buffer   = reinterpret_cast<VoxelDebug *>(u_pos.mapped);
        *buffer = voxel_options;
        buffer->size = voxel_size;
        buffer->res  = voxel_res;

        api.bind_buffer(generate_aniso_base, 0, u_pos);
    }

    // use the RGBA8 format defined at creation in view_formats
    {
        auto &image = api.get_image(voxels_radiance);
        transition_if_needed_internal(api, image, THSVS_ACCESS_ANY_SHADER_READ_SAMPLED_IMAGE_OR_UNIFORM_TEXEL_BUFFER, vk::ImageLayout::eShaderReadOnlyOptimal);
    }
    api.bind_combined_image_sampler(generate_aniso_base, 1, voxels_radiance, trilinear_sampler);

    std::vector<vk::ImageMemoryBarrier> barriers;

    {
        std::vector<vk::ImageView> views;
        views.reserve(voxels_directional_volumes.size());
        for (const auto& volume_h : voxels_directional_volumes)
        {
            auto &image = api.get_image(volume_h);
            transition_if_needed_internal(api, image, THSVS_ACCESS_GENERAL, vk::ImageLayout::eGeneral);

            views.push_back(api.get_image(volume_h).mip_views[0]);

            barriers.emplace_back();
            auto &image_barrier = barriers.back();
            image_barrier.srcAccessMask = vk::AccessFlagBits::eMemoryWrite;
            image_barrier.dstAccessMask = vk::AccessFlagBits::eMemoryRead;
            image_barrier.oldLayout = image.layout;
            image_barrier.newLayout = image.layout;
            image_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            image_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            image_barrier.image = image.vkhandle;
            image_barrier.subresourceRange = image.full_range;
        }
        api.bind_images(generate_aniso_base, 2, voxels_directional_volumes, views);
    }

    auto count = voxel_res / 8; // local compute size
    api.dispatch(generate_aniso_base, count, count, count);

    cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eBottomOfPipe, vk::DependencyFlagBits::eByRegion, {}, {}, barriers);

    for (uint mip_i = 0; count > 1; mip_i++)
    {
        count      /= 2;
        voxel_size *= 2;
        voxel_res  /= 2;

        auto src = mip_i;
        auto dst = mip_i + 1;

        // Bind voxel options
        {
            auto u_pos     = api.dynamic_uniform_buffer(sizeof(VoxelDebug));
            auto *buffer   = reinterpret_cast<VoxelDebug *>(u_pos.mapped);
            *buffer = voxel_options;
            buffer->size = voxel_size;
            buffer->res  = voxel_res;


            api.bind_buffer(generate_aniso_mipmap, 0, u_pos);
        }

        {
            auto u_pos     = api.dynamic_uniform_buffer(sizeof(int));
            auto *buffer   = reinterpret_cast<int*>(u_pos.mapped);
            *buffer = static_cast<int>(src);
            api.bind_buffer(generate_aniso_mipmap, 1, u_pos);
        }

        std::vector<vk::ImageView> src_views;
        std::vector<vk::ImageView> dst_views;
        src_views.reserve(voxels_directional_volumes.size());
        dst_views.reserve(voxels_directional_volumes.size());

        std::vector<vk::ImageMemoryBarrier> image_barriers;

        for (const auto& volume_h : voxels_directional_volumes)
        {
            auto &image = api.get_image(volume_h);
            src_views.push_back(image.mip_views[src]);
            dst_views.push_back(image.mip_views[dst]);

            transition_if_needed_internal(api, image, THSVS_ACCESS_GENERAL, vk::ImageLayout::eGeneral);
        }

        api.bind_images(generate_aniso_mipmap, 2, voxels_directional_volumes, src_views);
        api.bind_images(generate_aniso_mipmap, 3, voxels_directional_volumes, dst_views);

        api.dispatch(generate_aniso_mipmap, count, count, count);

        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eBottomOfPipe, vk::DependencyFlagBits::eByRegion, {}, {}, barriers);
    }

    api.end_label();
}

void Renderer::composite_hdr()
{
    static usize s_selected = 1;
    static float s_exposure = 1.0f;

    api.begin_label("Tonemap");

#if defined(ENABLE_IMGUI)
    if (p_ui->begin_window("HDR Shader"))
    {
        static std::array options{"Reinhard", "Exposure", "Clamp"};
        tools::imgui_select("Tonemap", options.data(), options.size(), s_selected);
        ImGui::SliderFloat("Exposure", &s_exposure, 0.0f, 1.0f);
        p_ui->end_window();
    }
#endif

    api.set_viewport_and_scissor(api.ctx.swapchain.extent.width, api.ctx.swapchain.extent.height);

    vulkan::PassInfo pass;
    pass.present = true;

    vulkan::AttachmentInfo color_info;
    color_info.load_op = vk::AttachmentLoadOp::eClear;
    color_info.rt      = swapchain_rt;
    pass.color         = std::make_optional(color_info);



    auto &hdr_rt = api.get_rendertarget(color_rt);
    api.bind_combined_image_sampler(hdr_compositing, vulkan::SHADER_DESCRIPTOR_SET, 0, hdr_rt.image_h, default_sampler);

    api.begin_pass(std::move(pass));

    // Make a shader debugging window and its own uniform buffer
    {
        auto u_pos   = api.dynamic_uniform_buffer(sizeof(uint) + sizeof(float));
        auto *buffer = reinterpret_cast<uint *>(u_pos.mapped);
        buffer[0] = static_cast<uint>(s_selected);
        auto *floatbuffer = reinterpret_cast<float*>(buffer + 1);
        floatbuffer[0] = s_exposure;
        api.bind_buffer(hdr_compositing, vulkan::SHADER_DESCRIPTOR_SET, 1, u_pos);
    }

    api.bind_program(hdr_compositing);


    api.draw(3, 1, 0, 0);

    api.end_pass();
    api.end_label();
}

void render_sky(Renderer &r)
{
    auto &api = r.api;

    auto &transmittance_lut_rt = api.get_rendertarget(r.sky.transmittance_lut_rt);
    auto &transmittance_lut = api.get_image(transmittance_lut_rt.image_h);

    auto &skyview_lut_rt = api.get_rendertarget(r.sky.skyview_lut_rt);
    auto &skyview_lut = api.get_image(skyview_lut_rt.image_h);

    api.begin_label("Transmittance LUT");

    assert_uniform_size(AtmosphereParameters);
    static_assert(sizeof(AtmosphereParameters) == 240);

    auto atmosphere_params = api.dynamic_uniform_buffer(sizeof(AtmosphereParameters));
    auto *p        = reinterpret_cast<AtmosphereParameters *>(atmosphere_params.mapped);

    {
        // info.solar_irradiance = { 1.474000f, 1.850400f, 1.911980f };
        p->solar_irradiance = {1.0f, 1.0f, 1.0f}; // Using a normalise sun illuminance. This is to make sure the LUTs acts as a transfert
                                                  // factor to apply the runtime computed sun irradiance over.
        p->sun_angular_radius = 0.004675f;

        // Earth
        p->bottom_radius = 6360000.0f;
        p->top_radius    = 6460000.0f;
        p->ground_albedo = {0.0f, 0.0f, 0.0f};

        // Raleigh scattering
        constexpr double kRayleighScaleHeight = 8000.0;
        constexpr double kMieScaleHeight      = 1200.0;

        p->rayleigh_density.width     = 0.0f;
        p->rayleigh_density.layers[0] = DensityProfileLayer{.exp_term = 1.0f, .exp_scale = -1.0f / kRayleighScaleHeight};
        p->rayleigh_scattering        = {0.000005802f, 0.000013558f, 0.000033100f};

        // Mie scattering
        p->mie_density.width  = 0.0f;
        p->mie_density.layers[0] = DensityProfileLayer{.exp_term = 1.0f, .exp_scale = -1.0f / kMieScaleHeight};

        p->mie_scattering        = {0.000003996f, 0.000003996f, 0.000003996f};
        p->mie_extinction        = {0.000004440f, 0.000004440f, 0.000004440f};
        p->mie_phase_function_g  = 0.8f;

        // Ozone absorption
        p->absorption_density.width  = 25000.0f;
        p->absorption_density.layers[0] = DensityProfileLayer{.linear_term =  1.0f / 15000.0f, .constant_term = -2.0f / 3.0f};

        p->absorption_density.layers[1] = DensityProfileLayer{.linear_term = -1.0f / 15000.0f, .constant_term =  8.0f / 3.0f};
        p->absorption_extinction      = {0.000000650f, 0.000001881f, 0.000000085f};

        const double max_sun_zenith_angle = PI * 120.0 / 180.0; // (use_half_precision_ ? 102.0 : 120.0) / 180.0 * kPi;
        p->mu_s_min                       = (float)cos(max_sun_zenith_angle);
    }


    /// --- Render transmittance LUT
    {
        api.set_viewport_and_scissor(transmittance_lut.info.width, transmittance_lut.info.height);

        vulkan::PassInfo pass;
        pass.present = false;

        vulkan::AttachmentInfo color_info;
        color_info.load_op = vk::AttachmentLoadOp::eClear;
        color_info.rt      = r.sky.transmittance_lut_rt;
        pass.color         = std::make_optional(color_info);

        api.begin_pass(std::move(pass));

        api.bind_buffer(r.sky.render_transmittance, vulkan::GLOBAL_DESCRIPTOR_SET, 0, r.global_uniform_pos);
        api.bind_buffer(r.sky.render_transmittance, vulkan::SHADER_DESCRIPTOR_SET, 0, atmosphere_params);
        api.bind_program(r.sky.render_transmittance);

        api.draw(3, 1, 0, 0);

        api.end_pass();
    }

    api.end_label();
    api.begin_label("Sky Multiscattering LUT");

    transition_if_needed_internal(api, transmittance_lut, THSVS_ACCESS_ANY_SHADER_READ_SAMPLED_IMAGE_OR_UNIFORM_TEXEL_BUFFER, vk::ImageLayout::eShaderReadOnlyOptimal);

    auto &multiscattering_lut = api.get_image(r.sky.multiscattering_lut);

    {
        auto &program = r.sky.compute_multiscattering_lut;

        api.bind_buffer(program, 0, atmosphere_params);

        api.bind_combined_image_sampler(program,
                                        1,
                                        transmittance_lut_rt.image_h,
                                        r.trilinear_sampler);

        {
            transition_if_needed_internal(api, multiscattering_lut, THSVS_ACCESS_GENERAL, vk::ImageLayout::eGeneral);
            api.bind_image(program, 2, r.sky.multiscattering_lut);
        }

        auto size_x = multiscattering_lut.info.width;
        auto size_y = multiscattering_lut.info.height;
        api.dispatch(program, size_x, size_y, 1);

        transition_if_needed_internal(api, multiscattering_lut, THSVS_ACCESS_ANY_SHADER_READ_SAMPLED_IMAGE_OR_UNIFORM_TEXEL_BUFFER, vk::ImageLayout::eShaderReadOnlyOptimal);
    }


    api.end_label();
    api.begin_label("SkyView LUT");

    /// --- Render SkyView LUT
    {
        auto &program = r.sky.render_skyview;

        api.set_viewport_and_scissor(skyview_lut.info.width, skyview_lut.info.height);

        vulkan::PassInfo pass;
        pass.present = false;

        vulkan::AttachmentInfo color_info;
        color_info.load_op = vk::AttachmentLoadOp::eClear;
        color_info.rt      = r.sky.skyview_lut_rt;
        pass.color         = std::make_optional(color_info);


        api.begin_pass(std::move(pass));

        api.bind_buffer(program, vulkan::GLOBAL_DESCRIPTOR_SET, 0, r.global_uniform_pos);
        api.bind_buffer(program, vulkan::SHADER_DESCRIPTOR_SET, 0, atmosphere_params);

        api.bind_combined_image_sampler(program,
                                        vulkan::SHADER_DESCRIPTOR_SET,
                                        1,
                                        transmittance_lut_rt.image_h,
                                        r.trilinear_sampler);

        api.bind_combined_image_sampler(program,
                                        vulkan::SHADER_DESCRIPTOR_SET,
                                        2,
                                        r.sky.multiscattering_lut,
                                        r.trilinear_sampler);

        api.bind_program(program);

        api.draw(3, 1, 0, 0);

        api.end_pass();
    }

    api.end_label();

#if defined(ENABLE_IMGUI)
    {

        if(r.p_ui->begin_window("Sky"))
        {
            float scale = 2.0f;
            ImGui::Text("Transmittance LUT:");
            ImGui::Image(reinterpret_cast<void *>(transmittance_lut_rt.image_h.value()),
                         ImVec2(scale * transmittance_lut.info.width, scale * transmittance_lut.info.height));

            scale = 10.f;
            ImGui::Text("Multiscattering LUT:");
            ImGui::Image(reinterpret_cast<void *>(r.sky.multiscattering_lut.value()),
                         ImVec2(scale * multiscattering_lut.info.width, scale * multiscattering_lut.info.height));

            scale = 2.f;
            ImGui::Text("SkyView LUT:");
            ImGui::Image(reinterpret_cast<void *>(skyview_lut_rt.image_h.value()),
                         ImVec2(scale * skyview_lut.info.width, scale * skyview_lut.info.height));

            r.p_ui->end_window();
        }
    }
#endif

    api.begin_label("Sky render");

    /// --- Raymarch the sky
    {
        api.set_viewport_and_scissor(api.ctx.swapchain.extent.width, api.ctx.swapchain.extent.height);

        vulkan::PassInfo pass;
        pass.present = false;

        vulkan::AttachmentInfo color_info;
        color_info.load_op = vk::AttachmentLoadOp::eLoad;
        color_info.rt      = r.color_rt;
        pass.color         = std::make_optional(color_info);

        transition_if_needed_internal(api,
                                      transmittance_lut,
                                      THSVS_ACCESS_ANY_SHADER_READ_SAMPLED_IMAGE_OR_UNIFORM_TEXEL_BUFFER,
                                      vk::ImageLayout::eShaderReadOnlyOptimal);
        transition_if_needed_internal(api, skyview_lut, THSVS_ACCESS_ANY_SHADER_READ_SAMPLED_IMAGE_OR_UNIFORM_TEXEL_BUFFER, vk::ImageLayout::eShaderReadOnlyOptimal);

        auto &program = r.sky.sky_raymarch;

        api.bind_buffer(program, vulkan::GLOBAL_DESCRIPTOR_SET, 0, r.global_uniform_pos);
        api.bind_buffer(program, vulkan::SHADER_DESCRIPTOR_SET, 0, atmosphere_params);

        api.bind_combined_image_sampler(program,
                                        vulkan::SHADER_DESCRIPTOR_SET,
                                        1,
                                        transmittance_lut_rt.image_h,
                                        r.trilinear_sampler);

        api.bind_combined_image_sampler(program,
                                        vulkan::SHADER_DESCRIPTOR_SET,
                                        2,
                                        skyview_lut_rt.image_h,
                                        r.trilinear_sampler);

        {
            auto &rt = api.get_rendertarget(r.depth_rt);
            auto &image = api.get_image(rt.image_h);
            transition_if_needed_internal(api,
                                          image,
                                          THSVS_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ,
                                          vk::ImageLayout::eDepthStencilReadOnlyOptimal);
            api.bind_combined_image_sampler(program, vulkan::SHADER_DESCRIPTOR_SET, 3, rt.image_h, r.nearest_sampler);
        }


        api.begin_pass(std::move(pass));

        api.bind_program(program);

        api.draw(3, 1, 0, 0);

        api.end_pass();
    }

    api.end_label();
}

void draw_fps(Renderer &renderer)
{
    auto &api = renderer.api;
    api.begin_label("Draw fps");

#if defined(ENABLE_IMGUI)
    ImGuiIO &io  = ImGui::GetIO();
    const auto &window = *renderer.p_window;
    const auto &timer = *renderer.p_timer;

    io.DeltaTime = timer.get_delta_time();
    io.Framerate = timer.get_average_fps();

    io.DisplaySize.x             = float(api.ctx.swapchain.extent.width);
    io.DisplaySize.y             = float(api.ctx.swapchain.extent.height);
    io.DisplayFramebufferScale.x = window.get_dpi_scale().x;
    io.DisplayFramebufferScale.y = window.get_dpi_scale().y;

    auto &ui = *renderer.p_ui;
    if (ui.begin_window("Stats"))
    {

        if (ImGui::Button("Dump usage"))
        {
            char *dump;
            vmaBuildStatsString(api.ctx.allocator, &dump, VK_TRUE);
            std::cout << "Vulkan memory dump:\n" << dump << "\n";

            std::ofstream file{"dump.json"};
            file << dump;

            vmaFreeStatsString(api.ctx.allocator, dump);
        }

        // 10 is the number of heaps of the device 10 should be fine?
        std::array<VmaBudget, 10> budgets{};
        vmaGetBudget(api.ctx.allocator, budgets.data());

        for (usize i = 0; i < budgets.size(); i++)
        {
            const auto &budget = budgets[i];
            if (budget.blockBytes == 0)
            {
                continue;
            }

            ImGui::Text("Heap #%llu", static_cast<u64>(i));
            ImGui::Text("Block bytes: %llu", static_cast<u64>(budget.blockBytes));
            ImGui::Text("Allocation bytes: %llu", static_cast<u64>(budget.allocationBytes));
            ImGui::Text("Usage: %llu", static_cast<u64>(budget.usage));
            ImGui::Text("Budget: %llu", static_cast<u64>(budget.budget));
            ImGui::Text("Total: %llu", static_cast<u64>(budget.usage + budget.budget));
            double utilization = 100.0 * budget.usage / (budget.usage + budget.budget);
            ImGui::Text("%02.2f%%", utilization);
        }

        ui.end_window();
    }

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
        if (timestamps.size() > 0)
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
            static float gpu_values[128];
            static float cpu_values[128];

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
            ImGui::PlotLines("", gpu_values, 128, 0, "", 0.0f, 30000.0f, ImVec2(0, 80));

            ImGui::Text("%-17s: %7.1f ms", "Total CPU time", cpu_average);
            ImGui::PlotLines("", cpu_values, 128, 0, "", 0.0f, 30000.0f, ImVec2(0, 80));
        }

        ui.end_window();
    }

#endif
    api.end_label();
}

void update_uniforms(Renderer &r)
{
    auto &api = r.api;
    api.begin_label("Update uniforms");
    float aspect_ratio = api.ctx.swapchain.extent.width / float(api.ctx.swapchain.extent.height);
    static float fov   = 60.0f;
    static float s_near  = 1.0f;
    static float s_far   = 200.0f;

    r.p_camera->perspective(fov, aspect_ratio, s_near, 200.f);
    r.p_camera->update_view();

    r.sun.position = float3(0.0f, 40.0f, 0.0f);
    r.sun.ortho_square(40.f, 1.f, 100.f);

    r.global_uniform_pos     = api.dynamic_uniform_buffer(sizeof(GlobalUniform));
    auto *globals            = reinterpret_cast<GlobalUniform *>(r.global_uniform_pos.mapped);
    std::memset(globals, 0, sizeof(GlobalUniform));

    globals->camera_pos      = r.p_camera->position;
    globals->camera_view     = r.p_camera->get_view();
    globals->camera_proj     = r.p_camera->get_projection();
    globals->camera_inv_proj = glm::inverse(globals->camera_proj);
    globals->camera_inv_view_proj = glm::inverse(globals->camera_proj * globals->camera_view);
    globals->sun_view        = r.sun.get_view();
    globals->sun_proj        = r.sun.get_projection();

    globals->resolution = uint2(api.ctx.swapchain.extent.width, api.ctx.swapchain.extent.height);
    globals->raymarch_min_max_spp = float2(4, 14);
    globals->MultipleScatteringFactor = 1.0f;
    globals->MultiScatteringLUTRes = 32;

    globals->TRANSMITTANCE_TEXTURE_WIDTH = 256;
    globals->TRANSMITTANCE_TEXTURE_HEIGHT = 64;

    globals->sun_direction = float4(-r.sun.front, 1);


    static float s_sun_illuminance = 1.0f;
    static float s_multiple_scattering = 0.0f;
    if (r.p_ui->begin_window("Globals"))
    {
        ImGui::SliderFloat("Near plane", &s_near, 0.01f, 1.f);
        ImGui::SliderFloat("Far plane", &s_far, 100.0f, 100000.0f);

        if (ImGui::Button("Reset near"))
        {
            s_near = 0.1f;
        }
        if (ImGui::Button("Reset far"))
        {
            s_far = 200.0f;
        }

        ImGui::SliderFloat("Sun illuminance", &s_sun_illuminance, 0.1f, 100.f);
        ImGui::SliderFloat("Multiple scattering", &s_multiple_scattering, 0.0f, 1.0f);
        r.p_ui->end_window();
    }
    globals->sun_illuminance     = s_sun_illuminance * float3(1.0f);
    globals->multiple_scattering = s_multiple_scattering;

    //
    // From AtmosphereParameters
    //
    const float EarthBottomRadius = 6360.0f;
    const float EarthTopRadius = 6460.0f;   // 100km atmosphere radius, less edge visible and it contain 99.99% of the atmosphere medium https://en.wikipedia.org/wiki/K%C3%A1rm%C3%A1n_line
    const float EarthRayleighScaleHeight = 8.0f;
    const float EarthMieScaleHeight = 1.2f;

    // Sun - This should not be part of the sky model...
    // info.solar_irradiance = { 1.474000f, 1.850400f, 1.911980f };
    globals->solar_irradiance = {1.0f, 1.0f, 1.0f}; // Using a normalise sun illuminance. This is to make sure the LUTs acts as a transfert
                              // factor to apply the runtime computed sun irradiance over.
    globals->sun_angular_radius = 0.004675f * 20.f;

    // Earth
    globals->bottom_radius = EarthBottomRadius;
    globals->top_radius    = EarthTopRadius;
    globals->ground_albedo = {0.0f, 0.0f, 0.0f};

    // Raleigh scattering
    globals->rayleigh_density[0] = {0.0f, 0.0f, 0.0f, 0.0f};
    globals->rayleigh_density[1] = {0.0f, 0.0f, 1.0f, -1.0f / EarthRayleighScaleHeight};
    globals->rayleigh_density[2] = {0.0f, 0.0f ,0.0f, 0.0f};
    globals->rayleigh_scattering        = {0.005802f, 0.013558f, 0.033100f}; // 1/km

    // Mie scattering
    globals->mie_density[0] = {0.0f, 0.0f, 0.0f, 0.0f};
    globals->mie_density[1] = {0.0f, 0.0f, 1.0f, -1.0f / EarthMieScaleHeight};
    globals->mie_density[2] = {0.0f, 0.0f ,0.0f, 0.0f};
    globals->mie_scattering        = {0.003996f, 0.003996f, 0.003996f}; // 1/km
    globals->mie_extinction        = {0.004440f, 0.004440f, 0.004440f}; // 1/km
    globals->mie_phase_function_g  = 0.8f;
    globals->mie_absorption = glm::max(globals->mie_extinction - globals->mie_scattering, float3(0.0f));

    // Ozone absorption
    globals->absorption_density[0] = {25.0f, 0.0f, 0.0f, 1.0f / 15.0f};
    globals->absorption_density[1] = {-2.0f / 3.0f, 0.0f, 0.0f, 0.0f};
    globals->absorption_density[2] = {-1.0f / 15.0f, 8.0f / 3.0f, 0.0f, 0.0f};
    globals->absorption_extinction        = {0.000650f, 0.001881f, 0.000085f}; // 1/km

    const double max_sun_zenith_angle = PI * 120.0 / 180.0; // (use_half_precision_ ? 102.0 : 120.0) / 180.0 * kPi;
    globals->mu_s_min                     = (float)cos(max_sun_zenith_angle);

    api.end_label();
}

void Renderer::draw()
{
    bool is_ok = api.start_frame();
    if (!is_ok) {
#if defined(ENABLE_IMGUI)
        ImGui::EndFrame();
#endif
        return;
    }

    draw_fps(*this);

    update_uniforms(*this);

    prepass(*this);

    api.begin_label("Clear voxels");
    vk::ClearColorValue clear{};
    clear.float32[0] = 0.f;
    clear.float32[1] = 0.f;
    clear.float32[2] = 0.f;
    clear.float32[3] = 0.f;
    api.clear_image(voxels_albedo, clear);
    api.clear_image(voxels_normal, clear);
    api.clear_image(voxels_radiance, clear);
    for (auto image_h : voxels_directional_volumes)
    {
        api.clear_image(image_h, clear);
    }
    api.end_label();

    voxelize_scene();
    inject_direct_lighting();
    generate_aniso_voxels();

    draw_floor(*this);
    draw_model();
    visualize_voxels();

    render_sky(*this);


    composite_hdr();

    imgui_draw();
    api.end_frame();
}

} // namespace my_app
