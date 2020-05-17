#include "renderer/renderer.hpp"
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
#include <iostream>
#include <sstream>


namespace my_app
{

struct ShaderDebug
{
    uint selected;
    float opacity;
};

Renderer Renderer::create(const Window &window, Camera &camera)
{
    Renderer r;
    r.api      = vulkan::API::create(window);
    r.p_camera = &camera;

    /// --- Init ImGui

#if defined(ENABLE_IMGUI)
    {
        ImGui::CreateContext();
        auto &style             = ImGui::GetStyle();
        style.FrameRounding     = 0.f;
        style.GrabRounding      = 0.f;
        style.WindowRounding    = 0.f;
        style.ScrollbarRounding = 0.f;
        style.GrabRounding      = 0.f;
        style.TabRounding       = 0.f;
    }

    {
        vulkan::GraphicsProgramInfo pinfo{};
        pinfo.vertex_shader   = r.api.create_shader("shaders/gui.vert.spv");
        pinfo.fragment_shader = r.api.create_shader("shaders/gui.frag.spv");
        // clang-format off
        pinfo.push_constant({/*.stages = */ vk::ShaderStageFlagBits::eVertex, /*.offset = */ 0, /*.size = */ 4 * sizeof(float)});
        pinfo.binding({/* .set = */ vulkan::SHADER_DESCRIPTOR_SET, /*.slot = */ 0, /*.stages = */ vk::ShaderStageFlagBits::eFragment, /*.type = */ vk::DescriptorType::eCombinedImageSampler, /*.count = */ 1});
        // clang-format on
        pinfo.vertex_stride(sizeof(ImDrawVert));

        pinfo.vertex_info({vk::Format::eR32G32Sfloat, MEMBER_OFFSET(ImDrawVert, pos)});
        pinfo.vertex_info({vk::Format::eR32G32Sfloat, MEMBER_OFFSET(ImDrawVert, uv)});
        pinfo.vertex_info({vk::Format::eR8G8B8A8Unorm, MEMBER_OFFSET(ImDrawVert, col)});
        pinfo.enable_depth = false;

        r.gui_program = r.api.create_program(std::move(pinfo));
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

        r.gui_texture = r.api.create_image(iinfo);
        r.api.upload_image(r.gui_texture, pixels, iinfo.width * iinfo.height * 4);
    }
#endif

    /// --- glTF Model

    {
        r.model = load_model("../models/Sponza/glTF/Sponza.gltf");
        r.load_model_data();
    }

    {
        vulkan::ImageInfo iinfo;
        iinfo.name   = "Depth texture";
        iinfo.format = vk::Format::eD32Sfloat;
        iinfo.width  = r.api.ctx.swapchain.extent.width;
        iinfo.height = r.api.ctx.swapchain.extent.height;
        iinfo.depth  = 1;
        iinfo.usages = vk::ImageUsageFlagBits::eDepthStencilAttachment;
        auto depth_h = r.api.create_image(iinfo);

        vulkan::RTInfo dinfo;
        dinfo.is_swapchain = false;
        dinfo.image_h      = depth_h;
        r.depth_rt         = r.api.create_rendertarget(dinfo);

        vulkan::RTInfo cinfo;
        cinfo.is_swapchain = true;
        r.color_rt         = r.api.create_rendertarget(cinfo);
    }

    /// --- Shadow map
    {
        r.sun = Camera::create(float3(0, 20, 0));
    }
    {
        vulkan::ImageInfo iinfo;
        iinfo.name   = "Shadow Map Depth";
        iinfo.format = vk::Format::eD32Sfloat;
        iinfo.width  = 4 * 1024;
        iinfo.height = 4 * 1024;
        iinfo.depth  = 1;
        iinfo.usages = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled;
        auto depth_h = r.api.create_image(iinfo);

        vulkan::RTInfo dinfo;
        dinfo.is_swapchain    = false;
        dinfo.image_h         = depth_h;
        r.shadow_map_depth_rt = r.api.create_rendertarget(dinfo);
    }

    {
        vulkan::GraphicsProgramInfo pinfo{};
        pinfo.vertex_shader = r.api.create_shader("shaders/gltf.vert.spv");

        // camera uniform buffer
        pinfo.binding({/*.set = */ vulkan::SHADER_DESCRIPTOR_SET, /*.slot = */ 0,
                       /*.stages = */ vk::ShaderStageFlagBits::eVertex,
                       /*.type = */ vk::DescriptorType::eUniformBufferDynamic, /*.count = */ 1});

        // node transform
        pinfo.binding({/*.set = */ vulkan::DRAW_DESCRIPTOR_SET, /*.slot = */ 0,
                       /*.stages = */ vk::ShaderStageFlagBits::eVertex,
                       /*.type = */ vk::DescriptorType::eUniformBufferDynamic, /*.count = */ 1});

        pinfo.vertex_stride(sizeof(GltfVertex));
        pinfo.vertex_info({vk::Format::eR32G32B32Sfloat, MEMBER_OFFSET(GltfVertex, position)});
        pinfo.vertex_info({vk::Format::eR32G32B32Sfloat, MEMBER_OFFSET(GltfVertex, normal)});
        pinfo.vertex_info({vk::Format::eR32G32Sfloat, MEMBER_OFFSET(GltfVertex, uv0)});
        pinfo.vertex_info({vk::Format::eR32G32Sfloat, MEMBER_OFFSET(GltfVertex, uv1)});
        pinfo.vertex_info({vk::Format::eR32G32B32A32Sfloat, MEMBER_OFFSET(GltfVertex, joint0)});
        pinfo.vertex_info({vk::Format::eR32G32B32A32Sfloat, MEMBER_OFFSET(GltfVertex, weight0)});
        pinfo.enable_depth = true;

        r.model_vertex_only = r.api.create_program(std::move(pinfo));
    }

    /// --- Voxelization
    {
        r.voxel_options.res = 512;

        vulkan::ImageInfo iinfo;
        iinfo.name       = "Voxels albedo";
        iinfo.type       = vk::ImageType::e3D;
        iinfo.format     = vk::Format::eR32Uint;
        iinfo.width      = r.voxel_options.res;
        iinfo.height     = r.voxel_options.res;
        iinfo.depth      = r.voxel_options.res;
        iinfo.usages     = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferDst;
        r.voxels_albedo = r.api.create_image(iinfo);

        iinfo.name = "Voxels normal";
        r.voxels_normal = r.api.create_image(iinfo);

        iinfo.name = "Voxels radiance";
        r.voxels_radiance = r.api.create_image(iinfo);
    }

    {
        vulkan::GraphicsProgramInfo pinfo{};
        pinfo.vertex_shader   = r.api.create_shader("shaders/voxelization.vert.spv");
        pinfo.geom_shader     = r.api.create_shader("shaders/voxelization.geom.spv");
        pinfo.fragment_shader = r.api.create_shader("shaders/voxelization.frag.spv");

        // voxel options
        pinfo.binding({/*.set = */ vulkan::SHADER_DESCRIPTOR_SET, /*.slot = */ 0,
                       /*.stages = */ vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment,
                       /*.type = */ vk::DescriptorType::eUniformBufferDynamic, /*.count = */ 1});

        // projection cameras
        pinfo.binding({/*.set = */ vulkan::SHADER_DESCRIPTOR_SET, /*.slot = */ 1,
                       /*.stages = */ vk::ShaderStageFlagBits::eGeometry,
                       /*.type = */ vk::DescriptorType::eUniformBufferDynamic, /*.count = */ 1});

        // voxels textures
        pinfo.binding({/*.set = */ vulkan::SHADER_DESCRIPTOR_SET, /*.slot = */ 2,
                       /*.stages = */ vk::ShaderStageFlagBits::eFragment,
                       /*.type = */ vk::DescriptorType::eStorageImage, /*.count = */ 1});
        pinfo.binding({/*.set = */ vulkan::SHADER_DESCRIPTOR_SET, /*.slot = */ 3,
                       /*.stages = */ vk::ShaderStageFlagBits::eFragment,
                       /*.type = */ vk::DescriptorType::eStorageImage, /*.count = */ 1});

        // node transform
        pinfo.binding({/*.set = */ vulkan::DRAW_DESCRIPTOR_SET, /*.slot = */ 0,
                       /*.stages = */ vk::ShaderStageFlagBits::eVertex,
                       /*.type = */ vk::DescriptorType::eUniformBufferDynamic, /*.count = */ 1});

        // color texture
        pinfo.binding({/*.set = */ vulkan::DRAW_DESCRIPTOR_SET, /*.slot = */ 1,
                       /*.stages = */ vk::ShaderStageFlagBits::eFragment,
                       /*.type = */ vk::DescriptorType::eCombinedImageSampler, /*.count = */ 1});

        // normal texture
        pinfo.binding({/*.set = */ vulkan::DRAW_DESCRIPTOR_SET, /*.slot = */ 2,
                       /*.stages = */ vk::ShaderStageFlagBits::eFragment,
                       /*.type = */ vk::DescriptorType::eCombinedImageSampler, /*.count = */ 1});

        pinfo.vertex_stride(sizeof(GltfVertex));
        pinfo.vertex_info({vk::Format::eR32G32B32Sfloat, MEMBER_OFFSET(GltfVertex, position)});
        pinfo.vertex_info({vk::Format::eR32G32B32Sfloat, MEMBER_OFFSET(GltfVertex, normal)});
        pinfo.vertex_info({vk::Format::eR32G32Sfloat, MEMBER_OFFSET(GltfVertex, uv0)});
        pinfo.vertex_info({vk::Format::eR32G32Sfloat, MEMBER_OFFSET(GltfVertex, uv1)});
        pinfo.vertex_info({vk::Format::eR32G32B32A32Sfloat, MEMBER_OFFSET(GltfVertex, joint0)});
        pinfo.vertex_info({vk::Format::eR32G32B32A32Sfloat, MEMBER_OFFSET(GltfVertex, weight0)});


        pinfo.enable_depth = false;
        pinfo.enable_conservative_rasterization = false;

        r.voxelization = r.api.create_program(std::move(pinfo));
    }

    {
        vulkan::GraphicsProgramInfo pinfo{};
        pinfo.vertex_shader   = r.api.create_shader("shaders/voxel_visualization.vert.spv");
        pinfo.fragment_shader = r.api.create_shader("shaders/voxel_visualization.frag.spv");

        // voxel options
        pinfo.binding({/*.set = */ vulkan::SHADER_DESCRIPTOR_SET, /*.slot = */ 0,
                       /*.stages = */ vk::ShaderStageFlagBits::eFragment,
                       /*.type = */ vk::DescriptorType::eUniformBufferDynamic, /*.count = */ 1});
        // camera
        pinfo.binding({/*.set = */ vulkan::SHADER_DESCRIPTOR_SET, /*.slot = */ 1,
                       /*.stages = */ vk::ShaderStageFlagBits::eFragment,
                       /*.type = */ vk::DescriptorType::eUniformBufferDynamic, /*.count = */ 1});
        // debug
        pinfo.binding({/*.set = */ vulkan::SHADER_DESCRIPTOR_SET, /*.slot = */ 2,
                       /*.stages = */ vk::ShaderStageFlagBits::eFragment,
                       /*.type = */ vk::DescriptorType::eUniformBufferDynamic, /*.count = */ 1});

        // voxels textures
        pinfo.binding({/*.set = */ vulkan::SHADER_DESCRIPTOR_SET, /*.slot = */ 3,
                       /*.stages = */ vk::ShaderStageFlagBits::eFragment,
                       /*.type = */ vk::DescriptorType::eStorageImage, /*.count = */ 1});
        pinfo.binding({/*.set = */ vulkan::SHADER_DESCRIPTOR_SET, /*.slot = */ 4,
                       /*.stages = */ vk::ShaderStageFlagBits::eFragment,
                       /*.type = */ vk::DescriptorType::eStorageImage, /*.count = */ 1});
        pinfo.binding({/*.set = */ vulkan::SHADER_DESCRIPTOR_SET, /*.slot = */ 5,
                       /*.stages = */ vk::ShaderStageFlagBits::eFragment,
                       /*.type = */ vk::DescriptorType::eStorageImage, /*.count = */ 1});

        pinfo.enable_depth = false;

        r.visualization = r.api.create_program(std::move(pinfo));
    }

    {
        vulkan::ComputeProgramInfo pinfo{};
        pinfo.shader = r.api.create_shader("shaders/voxel_inject_direct_lighting.comp.spv");

        // voxel options
        pinfo.binding({/*.set = */ vulkan::SHADER_DESCRIPTOR_SET, /*.slot = */ 0,
                       /*.stages = */ vk::ShaderStageFlagBits::eCompute,
                       /*.type = */ vk::DescriptorType::eUniformBufferDynamic, /*.count = */ 1});

        // directional light
        pinfo.binding({/*.set = */ vulkan::SHADER_DESCRIPTOR_SET, /*.slot = */ 1,
                       /*.stages = */ vk::ShaderStageFlagBits::eCompute,
                       /*.type = */ vk::DescriptorType::eUniformBufferDynamic, /*.count = */ 1});

        // voxels textures
        pinfo.binding({/*.set = */ vulkan::SHADER_DESCRIPTOR_SET, /*.slot = */ 2,
                       /*.stages = */ vk::ShaderStageFlagBits::eCompute,
                       /*.type = */ vk::DescriptorType::eStorageImage, /*.count = */ 1});
        pinfo.binding({/*.set = */ vulkan::SHADER_DESCRIPTOR_SET, /*.slot = */ 3,
                       /*.stages = */ vk::ShaderStageFlagBits::eCompute,
                       /*.type = */ vk::DescriptorType::eStorageImage, /*.count = */ 1});
        pinfo.binding({/*.set = */ vulkan::SHADER_DESCRIPTOR_SET, /*.slot = */ 4,
                       /*.stages = */ vk::ShaderStageFlagBits::eCompute,
                       /*.type = */ vk::DescriptorType::eStorageImage, /*.count = */ 1});

        r.inject_direct_lighting = r.api.create_program(std::move(pinfo));
    }

    return r;
}

void Renderer::destroy()
{
    api.wait_idle();
    destroy_model();

    {
        auto &depth = api.get_rendertarget(shadow_map_depth_rt);
        api.destroy_image(depth.image_h);
    }

    {
        auto &depth = api.get_rendertarget(depth_rt);
        api.destroy_image(depth.image_h);
    }

    api.destroy_image(voxels_albedo);
    api.destroy_image(voxels_normal);
    api.destroy_image(voxels_radiance);

    api.destroy_image(gui_texture);
    api.destroy();
}

void Renderer::on_resize(int width, int height)
{
    api.on_resize(width, height);

    auto &depth = api.get_rendertarget(depth_rt);
    api.destroy_image(depth.image_h);

    vulkan::ImageInfo iinfo;
    iinfo.name    = "Depth texture";
    iinfo.format  = vk::Format::eD32Sfloat;
    iinfo.width   = api.ctx.swapchain.extent.width;
    iinfo.height  = api.ctx.swapchain.extent.height;
    iinfo.depth   = 1;
    iinfo.usages  = vk::ImageUsageFlagBits::eDepthStencilAttachment;
    depth.image_h = api.create_image(iinfo);
}

void Renderer::wait_idle()
{
    api.wait_idle();
}

void Renderer::reload_shader(const char *prefix_path, const Event &shader_event)
{
    std::stringstream shader_name_stream;
    shader_name_stream << prefix_path << '/' << shader_event.name;
    std::string shader_name = shader_name_stream.str();

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

        if (program.info.fragment_shader.is_valid()) {
            auto &fragment_shader = api.get_shader(program.info.fragment_shader);
            if (fragment_shader.name == shader.name) {
                to_remove.push_back(program.info.fragment_shader);
                program.info.fragment_shader = new_shader;
            }
        }
    }

    assert(to_remove.size() > 0);

    // Destroy the old shaders
    for (vulkan::ShaderH shader_h : to_remove) {
        std::cerr << "Removing handle: " << shader_h.value() << "\n";
        api.destroy_shader(shader_h);
    }
}

void Renderer::imgui_draw()
{
    api.begin_label("ImGui");
#if defined(ENABLE_IMGUI)
    ImGui::ShowStyleEditor();
    ImGui::Render();
    ImDrawData *data = ImGui::GetDrawData();
    if (data == nullptr || data->TotalVtxCount == 0) {
        return;
    }

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

    // Render GUI
    i32 vertex_offset = 0;
    u32 index_offset  = 0;
    for (int list = 0; list < data->CmdListsCount; list++) {
        const ImDrawList *cmd_list = data->CmdLists[list];

        for (int command_index = 0; command_index < cmd_list->CmdBuffer.Size; command_index++) {
            const ImDrawCmd *draw_command = &cmd_list->CmdBuffer[command_index];

            if (draw_command->TextureId) {
                auto texture = vulkan::ImageH(reinterpret_cast<u32>(draw_command->TextureId));
                api.bind_image(gui_program, vulkan::SHADER_DESCRIPTOR_SET, 0, texture);
            }
            else {
                api.bind_image(gui_program, vulkan::SHADER_DESCRIPTOR_SET, 0, gui_texture);
            }

            api.bind_program(gui_program);
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
#endif
    api.end_label();
}

static void bind_texture(Renderer &r, vulkan::GraphicsProgramH program_h, uint slot, std::optional<u32> i_texture)
{
    if (i_texture) {
        auto &texture = r.model.textures[*i_texture];
        auto &image   = r.model.images[texture.image];
        auto &sampler = r.model.samplers[texture.sampler];

        r.api.bind_combined_image_sampler(program_h, vulkan::DRAW_DESCRIPTOR_SET, slot, image.image_h, sampler.sampler_h);
    }
    else {
        // bind empty texture
    }
}

static void draw_node(Renderer &r, Node &node)
{
    if (node.dirty) {
        node.dirty            = false;
        auto translation      = glm::translate(glm::mat4(1.0f), node.translation);
        auto rotation         = glm::mat4(node.rotation);
        auto scale            = glm::scale(glm::mat4(1.0f), node.scale);
        node.cached_transform = translation * rotation * scale;
    }

    auto u_pos   = r.api.dynamic_uniform_buffer(sizeof(float4x4));
    auto *buffer = reinterpret_cast<float4x4 *>(u_pos.mapped);
    *buffer      = node.cached_transform;
    r.api.bind_buffer(r.model.program, vulkan::DRAW_DESCRIPTOR_SET, 0, u_pos);

    const auto &mesh = r.model.meshes[node.mesh];
    for (const auto &primitive : mesh.primitives) {
        // if program != last program then bind program

        const auto &material = r.model.materials[primitive.material];

        MaterialPushConstant material_pc = MaterialPushConstant::from(material);
        r.api.push_constant(vk::ShaderStageFlagBits::eFragment, 0, sizeof(material_pc), &material_pc);

        bind_texture(r, r.model.program, 1, material.base_color_texture);
        bind_texture(r, r.model.program, 2, material.normal_texture);
        bind_texture(r, r.model.program, 3, material.metallic_roughness_texture);

        r.api.draw_indexed(primitive.index_count, 1, primitive.first_index, static_cast<i32>(primitive.first_vertex), 0);
    }

    // TODO: transform relative to parent
    for (auto child_i : node.children) {
        draw_node(r, r.model.nodes[child_i]);
    }
}

void Renderer::draw_model()
{
    static usize s_selected = 0;
    static float s_opacity = 0.5f;
#if defined(ENABLE_IMGUI)
    ImGui::Begin("glTF Shader");
    ImGui::SliderFloat("Output opacity", &s_opacity, 0.0f, 1.0f);
    static std::array options{"Nothing", "BaseColor", "Normal", "MetallicRoughness", "ShadowMap", "LightPosition", "LightPos <= ShadowMap"};
    tools::imgui_select("Debug output", options.data(), options.size(), s_selected);
    ImGui::End();
#endif
    if (s_opacity == 0.0f) {
        return;
    }

    api.begin_label("Model");

    vk::Viewport viewport{};
    viewport.width    = api.ctx.swapchain.extent.width;
    viewport.height   = api.ctx.swapchain.extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    api.set_viewport(viewport);

    vk::Rect2D scissor{};
    scissor.extent = api.ctx.swapchain.extent;
    api.set_scissor(scissor);

    // Bind camera uniform buffer
    {
        float aspect_ratio = api.ctx.swapchain.extent.width / float(api.ctx.swapchain.extent.height);
        static float fov   = 60.0f;

        auto u_pos   = api.dynamic_uniform_buffer(2 * sizeof(float4x4));
        auto *buffer = reinterpret_cast<float4x4 *>(u_pos.mapped);
        buffer[0]    = p_camera->get_view();
        buffer[1]    = p_camera->perspective(fov, aspect_ratio, 0.1f, 30.f);
        buffer[2]    = sun.get_view();
        buffer[3]    = sun.get_projection();

        api.bind_buffer(model.program, vulkan::SHADER_DESCRIPTOR_SET, 0, u_pos);
    }

    // Make a shader debugging window and its own uniform buffer
    {
        auto u_pos   = api.dynamic_uniform_buffer(sizeof(ShaderDebug));
        auto *buffer = reinterpret_cast<ShaderDebug *>(u_pos.mapped);
        buffer->selected = static_cast<uint>(s_selected);
        buffer->opacity  = s_opacity;
        api.bind_buffer(model.program, vulkan::SHADER_DESCRIPTOR_SET, 1, u_pos);
    }

    // Bind the shadow map the shader
    {
        auto &shadow_map = api.get_rendertarget(shadow_map_depth_rt);
        api.bind_image(model.program, vulkan::SHADER_DESCRIPTOR_SET, 2, shadow_map.image_h);
    }

    api.bind_program(model.program);
    api.bind_index_buffer(model.index_buffer);
    api.bind_vertex_buffer(model.vertex_buffer);

    for (usize node_i : model.scene) {
        draw_node(*this, model.nodes[node_i]);
    }

    api.end_label();
}

static void draw_node_shadow(Renderer &r, Node &node)
{
    if (node.dirty) {
        node.dirty            = false;
        auto translation      = glm::translate(glm::mat4(1.0f), node.translation);
        auto rotation         = glm::mat4(node.rotation);
        auto scale            = glm::scale(glm::mat4(1.0f), node.scale);
        node.cached_transform = translation * rotation * scale;
    }

    auto u_pos   = r.api.dynamic_uniform_buffer(sizeof(float4x4));
    auto *buffer = reinterpret_cast<float4x4 *>(u_pos.mapped);
    *buffer      = node.cached_transform;
    r.api.bind_buffer(r.model_vertex_only, vulkan::DRAW_DESCRIPTOR_SET, 0, u_pos);

    const auto &mesh = r.model.meshes[node.mesh];
    for (const auto &primitive : mesh.primitives) {
        r.api.draw_indexed(primitive.index_count, 1, primitive.first_index, static_cast<i32>(primitive.first_vertex), 0);
    }

    // TODO: transform relative to parent
    for (auto child_i : node.children) {
        draw_node(r, r.model.nodes[child_i]);
    }
}

static void shadow_map(Renderer &r)
{
    r.api.begin_label("Shadow Map");
    vulkan::PassInfo pass;
    pass.present = false;

    vulkan::AttachmentInfo depth_info;
    depth_info.load_op = vk::AttachmentLoadOp::eClear;
    depth_info.rt      = r.shadow_map_depth_rt;
    pass.depth         = std::make_optional(depth_info);

    r.api.begin_pass(std::move(pass));

    auto shadow_map_rt  = r.api.get_rendertarget(r.shadow_map_depth_rt);
    auto shadow_map_img = r.api.get_image(shadow_map_rt.image_h);

    vk::Viewport viewport{};
    viewport.width    = shadow_map_img.image_info.extent.width;
    viewport.height   = shadow_map_img.image_info.extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    r.api.set_viewport(viewport);

    vk::Rect2D scissor{};
    scissor.extent.width  = shadow_map_img.image_info.extent.width;
    scissor.extent.height = shadow_map_img.image_info.extent.height;
    r.api.set_scissor(scissor);

    {
#if defined(ENABLE_IMGUI)
        ImGui::Begin("Sun");
#endif
        static float3 s_sun_angles = {90.0f, 0.0f, 0.0f};
        static float s_size        = 40.f;
        static float s_near        = 1.f;
        static float s_far         = 30.f;
#if defined(ENABLE_IMGUI)
        ImGui::SliderFloat3("Rotation", &s_sun_angles[0], -180.0f, 180.0f);
        ImGui::SliderFloat("Size", &s_size, 5.0f, 50.0f);
        ImGui::SliderFloat("Near", &s_near, 1.0f, 50.0f);
        ImGui::SliderFloat("Far", &s_far, 1.0f, 50.0f);
#endif

        bool dirty = false;
        if (s_sun_angles[0] != r.sun.pitch) {
            r.sun.pitch = s_sun_angles[0];
            dirty       = true;
        }
        if (s_sun_angles[1] != r.sun.yaw) {
            r.sun.yaw = s_sun_angles[1];
            dirty     = true;
        }
        if (s_sun_angles[2] != r.sun.roll) {
            r.sun.roll = s_sun_angles[2];
            dirty      = true;
        }
        if (dirty) {
            r.sun.update_view();
        }

#if defined(ENABLE_IMGUI)
        ImGui::End();
#endif

        auto u_pos   = r.api.dynamic_uniform_buffer(4 * sizeof(float4x4));
        auto *buffer = reinterpret_cast<float4x4 *>(u_pos.mapped);
        buffer[0]    = r.sun.get_view();
        buffer[1]    = r.sun.ortho_square(s_size, s_near, s_far);
        r.api.bind_buffer(r.model_vertex_only, vulkan::SHADER_DESCRIPTOR_SET, 0, u_pos);
    }

    r.api.bind_program(r.model_vertex_only);
    r.api.bind_index_buffer(r.model.index_buffer);
    r.api.bind_vertex_buffer(r.model.vertex_buffer);

    for (usize node_i : r.model.scene) {
        draw_node_shadow(r, r.model.nodes[node_i]);
    }

    r.api.end_pass();
    r.api.end_label();

#if defined(ENABLE_IMGUI)
    auto &depth = r.api.get_rendertarget(r.shadow_map_depth_rt);
    ImGui::Begin("Shadow map");
    ImGui::Image(reinterpret_cast<void *>(depth.image_h.value()), ImVec2(256, 256));
    ImGui::End();
#endif
}

static void voxelize_node(Renderer &r, Node &node)
{
    if (node.dirty) {
        node.dirty            = false;
        auto translation      = glm::translate(glm::mat4(1.0f), node.translation);
        auto rotation         = glm::mat4(node.rotation);
        auto scale            = glm::scale(glm::mat4(1.0f), node.scale);
        node.cached_transform = translation * rotation * scale;
    }

    auto u_pos   = r.api.dynamic_uniform_buffer(sizeof(float4x4));
    auto *buffer = reinterpret_cast<float4x4 *>(u_pos.mapped);
    *buffer      = node.cached_transform;
    r.api.bind_buffer(r.voxelization, vulkan::DRAW_DESCRIPTOR_SET, 0, u_pos);

    const auto &mesh = r.model.meshes[node.mesh];
    for (const auto &primitive : mesh.primitives) {
        // if program != last program then bind program

        const auto &material = r.model.materials[primitive.material];

        bind_texture(r, r.voxelization, 1, material.base_color_texture);
        bind_texture(r, r.voxelization, 2, material.normal_texture);

        r.api.draw_indexed(primitive.index_count, 1, primitive.first_index, static_cast<i32>(primitive.first_vertex), 0);
    }

    // TODO: transform relative to parent
    for (auto child_i : node.children) {
        draw_node(r, r.model.nodes[child_i]);
    }
}


void Renderer::voxelize_scene()
{
    api.begin_label("Voxelization");

    vk::Viewport viewport{};
    viewport.width    = voxel_options.res;
    viewport.height   = voxel_options.res;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    api.set_viewport(viewport);

    vk::Rect2D scissor{};
    scissor.extent.width  = voxel_options.res;
    scissor.extent.height = voxel_options.res;
    api.set_scissor(scissor);

    // Bind voxel debug
    {
#if defined(ENABLE_IMGUI)
        ImGui::Begin("Voxelization");
        ImGui::SliderFloat3("Center", &voxel_options.center[0], -40.f, 40.f);
        voxel_options.center = glm::floor(voxel_options.center);
        ImGui::SliderFloat("Voxel size (m)", &voxel_options.size, 0.01f, 0.1f);
        ImGui::End();
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
    api.bind_image(voxelization, vulkan::SHADER_DESCRIPTOR_SET, 2, voxels_albedo);
    api.bind_image(voxelization, vulkan::SHADER_DESCRIPTOR_SET, 3, voxels_normal);

    // TODO: triche
    api.bind_image(visualization, vulkan::SHADER_DESCRIPTOR_SET, 5, voxels_radiance);


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
    static usize s_selected = 1;
    static float s_opacity = 0.5f;
#if defined(ENABLE_IMGUI)
    ImGui::Begin("Voxels Shader");
    ImGui::SliderFloat("Output opacity", &s_opacity, 0.0f, 1.0f);
    static std::array options{"Nothing", "Albedo", "Normal", "Radiance"};
    tools::imgui_select("Debug output", options.data(), options.size(), s_selected);
    ImGui::End();
#endif
    if (s_opacity == 0.0f || s_selected == 0) {
        return;
    }

    api.begin_label("Voxel visualization");

    vk::Viewport viewport{};
    viewport.width    = api.ctx.swapchain.extent.width;
    viewport.height   = api.ctx.swapchain.extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    api.set_viewport(viewport);

    vk::Rect2D scissor{};
    scissor.extent = api.ctx.swapchain.extent;
    api.set_scissor(scissor);

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

    api.bind_image(visualization, vulkan::SHADER_DESCRIPTOR_SET, 3, voxels_albedo);
    api.bind_image(visualization, vulkan::SHADER_DESCRIPTOR_SET, 4, voxels_normal);
    api.bind_image(visualization, vulkan::SHADER_DESCRIPTOR_SET, 5, voxels_radiance);


    api.bind_program(visualization);
    api.draw(6, 1, 0, 0);
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

#define VOXEL 1

    shadow_map(*this);
#if !VOXEL

#else
    vk::ClearColorValue clear{};
    clear.float32[0] = 0.f;
    clear.float32[1] = 0.f;
    clear.float32[2] = 0.f;
    clear.float32[3] = 0.f;
    api.clear_image(voxels_albedo, clear);
    api.clear_image(voxels_normal, clear);
    api.clear_image(voxels_radiance, clear);

    voxelize_scene();
#endif

    vulkan::PassInfo pass;
    pass.present = true;

    vulkan::AttachmentInfo color_info;
    color_info.load_op = vk::AttachmentLoadOp::eClear;
    color_info.rt      = color_rt;
    pass.color         = std::make_optional(color_info);

    vulkan::AttachmentInfo depth_info;
    depth_info.load_op = vk::AttachmentLoadOp::eClear;
    depth_info.rt      = depth_rt;
    pass.depth         = std::make_optional(depth_info);
#if !VOXEL
#endif

    api.begin_pass(std::move(pass));

    draw_model();
#if !VOXEL
#else
    visualize_voxels();
#endif
    imgui_draw();

    api.end_pass();
    api.end_frame();
}

} // namespace my_app
