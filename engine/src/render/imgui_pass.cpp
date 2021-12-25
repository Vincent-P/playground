#include "render/imgui_pass.h"

#include "render/base_renderer.h"
#include "render/vulkan/commands.h"
namespace gfx = vulkan;

#include <imgui.h>
#include <Tracy.hpp>

void imgui_pass_init(gfx::Device &device, ImGuiPass &pass, VkFormat color_attachment_format)
{
    gfx::DescriptorType one_dynamic_buffer_descriptor = {{{.count = 1, .type = gfx::DescriptorType::DynamicBuffer}}};

    gfx::GraphicsState gui_state = {};
    gui_state.vertex_shader      = device.create_shader("C:/Users/vince/Documents/code/test-vulkan/build/msvc/shaders/gui.vert.glsl.spv");
    gui_state.fragment_shader    = device.create_shader("C:/Users/vince/Documents/code/test-vulkan/build/msvc/shaders/gui.frag.glsl.spv");
    gui_state.attachments_format = {.attachments_format = {color_attachment_format}};
    gui_state.descriptors.push_back(one_dynamic_buffer_descriptor);
    pass.program = device.create_program("imgui", gui_state);

    gfx::RenderState state = {.rasterization = {.culling = false}, .alpha_blending = true};
    device.compile(pass.program, state);

    auto & io     = ImGui::GetIO();
    uchar *pixels = nullptr;
    int    width  = 0;
    int    height = 0;
    io.Fonts->Build();
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    pass.font_atlas = device.create_image({
        .name   = "Font Atlas",
        .size   = {width, height, 1},
        .format = VK_FORMAT_R8G8B8A8_UNORM,
    });

    ImGui::GetIO().Fonts->SetTexID((void *)((u64)device.get_image_sampled_index(pass.font_atlas)));
}

void imgui_pass_draw(BaseRenderer &renderer, ImGuiPass &pass, gfx::GraphicsWork &cmd, Handle<gfx::Framebuffer> framebuffer)
{
    auto  current_frame = renderer.frame_count % FRAME_QUEUE_LENGTH;
    auto &timings       = renderer.timings[current_frame];
    auto &device        = renderer.device;

    ZoneNamedN(render_imgui, "ImGui drawing", true);
    timings.begin_label(cmd, "ImGui drawing");
    cmd.begin_debug_label("ImGui drawing");
    ImDrawData *data = ImGui::GetDrawData();

    // -- Prepare Imgui draw commands
    ASSERT(sizeof(ImDrawVert) * static_cast<u32>(data->TotalVtxCount) < 1_MiB);
    ASSERT(sizeof(ImDrawIdx) * static_cast<u32>(data->TotalVtxCount) < 1_MiB);

    u32 vertices_size = static_cast<u32>(data->TotalVtxCount) * sizeof(ImDrawVert);
    u32 indices_size  = static_cast<u32>(data->TotalIdxCount) * sizeof(ImDrawIdx);

    auto [p_vertices, vert_offset] = renderer.dynamic_vertex_buffer.allocate(device, vertices_size);
    auto *vertices                 = reinterpret_cast<ImDrawVert *>(p_vertices);

    auto [p_indices, ind_offset] = renderer.dynamic_index_buffer.allocate(device, indices_size);
    auto *indices                = reinterpret_cast<ImDrawIdx *>(p_indices);

    struct ImguiDrawCommand
    {
        u32      texture_id;
        u32      vertex_count;
        u32      index_offset;
        i32      vertex_offset;
        VkRect2D scissor;
    };

    static Vec<ImguiDrawCommand> draws;
    draws.clear();

    float2 clip_off   = data->DisplayPos;       // (0,0) unless using multi-viewports
    float2 clip_scale = data->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

    u32 i_draw        = 0;
    i32 vertex_offset = 0;
    u32 index_offset  = 0;
    for (int i = 0; i < data->CmdListsCount; i++)
    {
        const auto &cmd_list = *data->CmdLists[i];

        for (int i_vertex = 0; i_vertex < cmd_list.VtxBuffer.Size; i_vertex++)
        {
            vertices[i_vertex] = cmd_list.VtxBuffer.Data[i_vertex];
        }

        for (int i_index = 0; i_index < cmd_list.IdxBuffer.Size; i_index++)
        {
            indices[i_index] = cmd_list.IdxBuffer.Data[i_index];
        }

        vertices += cmd_list.VtxBuffer.Size;
        indices += cmd_list.IdxBuffer.Size;

        // Check that texture are correct
        for (int command_index = 0; command_index < cmd_list.CmdBuffer.Size; command_index++)
        {
            const auto &draw_command = cmd_list.CmdBuffer[command_index];

            // Prepare the texture to be sampled
            u32 texture_id = static_cast<u32>((u64)draw_command.TextureId);

            // Project scissor/clipping rectangles into framebuffer space
            ImVec4 clip_rect;
            clip_rect.x = (draw_command.ClipRect.x - clip_off.x) * clip_scale.x;
            clip_rect.y = (draw_command.ClipRect.y - clip_off.y) * clip_scale.y;
            clip_rect.z = (draw_command.ClipRect.z - clip_off.x) * clip_scale.x;
            clip_rect.w = (draw_command.ClipRect.w - clip_off.y) * clip_scale.y;

            // Apply scissor/clipping rectangle
            VkRect2D scissor;
            scissor.offset.x      = (static_cast<i32>(clip_rect.x) > 0) ? static_cast<i32>(clip_rect.x) : 0;
            scissor.offset.y      = (static_cast<i32>(clip_rect.y) > 0) ? static_cast<i32>(clip_rect.y) : 0;
            scissor.extent.width  = static_cast<u32>(clip_rect.z - clip_rect.x);
            scissor.extent.height = static_cast<u32>(clip_rect.w - clip_rect.y);

            draws.push_back({.texture_id    = texture_id,
                             .vertex_count  = draw_command.ElemCount,
                             .index_offset  = index_offset,
                             .vertex_offset = vertex_offset,
                             .scissor       = scissor});
            i_draw += 1;

            index_offset += draw_command.ElemCount;
        }
        vertex_offset += cmd_list.VtxBuffer.Size;
    }

    // -- Rendering
    PACKED(struct ImguiOptions {
        float2 scale;
        float2 translation;
        u64    vertices_pointer;
        u32    first_vertex;
        u32    vertices_descriptor_index;
    })

    auto *options = renderer.bind_shader_options<ImguiOptions>(cmd, pass.program);
    std::memset(options, 0, sizeof(ImguiOptions));
    options->scale = float2(2.0f / data->DisplaySize.x, 2.0f / data->DisplaySize.y);
    options->translation = float2(-1.0f - data->DisplayPos.x * options->scale.x, -1.0f - data->DisplayPos.y * options->scale.y);
    options->vertices_pointer          = 0;
    options->first_vertex              = static_cast<u32>(vert_offset / sizeof(ImDrawVert));
    options->vertices_descriptor_index = device.get_buffer_storage_index(renderer.dynamic_vertex_buffer.buffer);

    // Barriers
    for (i_draw = 0; i_draw < draws.size(); i_draw += 1)
    {
        const auto &draw = draws[i_draw];
        cmd.barrier(device.get_global_sampled_image(draw.texture_id), gfx::ImageUsage::GraphicsShaderRead);
    }

    // Draw pass
    auto &fb = *device.framebuffers.get(framebuffer);
    cmd.barrier(fb.color_attachments[0], gfx::ImageUsage::ColorAttachment);
    if (fb.depth_attachment.is_valid()) {
        cmd.barrier(fb.depth_attachment, gfx::ImageUsage::DepthAttachment);
    }
    cmd.begin_pass(framebuffer, std::array{gfx::LoadOp::load()});

    VkViewport viewport{};
    viewport.width    = data->DisplaySize.x * data->FramebufferScale.x;
    viewport.height   = data->DisplaySize.y * data->FramebufferScale.y;
    viewport.minDepth = 1.0f;
    viewport.maxDepth = 1.0f;
    cmd.set_viewport(viewport);

    cmd.bind_pipeline(pass.program, 0);
    cmd.bind_index_buffer(renderer.dynamic_index_buffer.buffer, VK_INDEX_TYPE_UINT16, ind_offset);

    for (i_draw = 0; i_draw < draws.size(); i_draw += 1)
    {
        const auto &draw = draws[i_draw];
        cmd.set_scissor(draw.scissor);

        u32 constants[] = {i_draw, draw.texture_id};
        cmd.push_constant(constants, sizeof(constants));
        cmd.draw_indexed({.vertex_count  = draw.vertex_count,
                          .index_offset  = draw.index_offset,
                          .vertex_offset = draw.vertex_offset});
    }

    cmd.end_pass();
    cmd.end_debug_label();
    timings.end_label(cmd);
}
