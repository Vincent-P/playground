#include "render/imgui_pass.hpp"
#include "render/hl_api.hpp"

#include <imgui/imgui.h>
#include <fmt/core.h>

namespace my_app
{

void create_imgui_pass(ImGuiPass &pass, RenderGraph &graph, vulkan::API &api)
{
    // Create vulkan programs
    vulkan::GraphicsProgramInfo pinfo{};
    pinfo.vertex_shader   = api.create_shader("shaders/gui.vert.spv");
    pinfo.fragment_shader = api.create_shader("shaders/gui.frag.spv");

    pinfo.vertex_stride(sizeof(ImDrawVert));
    pinfo.vertex_info({.format = VK_FORMAT_R32G32_SFLOAT, .offset = MEMBER_OFFSET(ImDrawVert, pos)});
    pinfo.vertex_info({.format = VK_FORMAT_R32G32_SFLOAT, .offset = MEMBER_OFFSET(ImDrawVert, uv)});
    pinfo.vertex_info({.format = VK_FORMAT_R8G8B8A8_UNORM, .offset = MEMBER_OFFSET(ImDrawVert, col)});

    pinfo.rasterization.culling = false;
    pinfo.alpha_blending = true;

    vulkan::GraphicsProgramInfo puintinfo = pinfo;
    puintinfo.fragment_shader             = api.create_shader("shaders/gui_uint.frag.spv");

    pass.float_program = api.create_program(std::move(pinfo));
    pass.uint_program  = api.create_program(std::move(puintinfo));

    // Upload the font atlas to the GPU
    uchar *pixels = nullptr;

    int w = 0;
    int h = 0;
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);

    pass.font_atlas = api.create_image({
        .name   = "ImGui font atlas",
        .width  = static_cast<uint>(w),
        .height = static_cast<uint>(h),
    });


    vulkan::ImageH *font_atlas_h = &pass.font_atlas;
    graph.add_pass({
        .name             = "Upload random rotations",
        .type             = PassType::Compute,
        .exec =
            [=](RenderGraph & /*graph*/, RenderPass & /*self*/, vulkan::API &api) {
                api.upload_image(*font_atlas_h, pixels, w * h * 4);
            },
    });
}

void add_imgui_pass(RenderGraph &graph, ImGuiPass &pass_data, ImageDescH output)
{
    ImGui::Render();
    ImDrawData *data = ImGui::GetDrawData();
    if (data == nullptr || data->TotalVtxCount == 0)
    {
        return;
    }

    auto &api = *graph.p_api;

    // The render graph needs to know about external images to put barriers on them correctly
    // are external images always going to be sampled or they need to be in differents categories
    // like regular images from the graph?
    Vec<vulkan::ImageH> external_images;
    external_images.push_back(pass_data.font_atlas);

    for (int list = 0; list < data->CmdListsCount; list++)
    {
        const auto &cmd_list = *data->CmdLists[list];

        for (int command_index = 0; command_index < cmd_list.CmdBuffer.Size; command_index++)
        {
            const auto &draw_command = cmd_list.CmdBuffer[command_index];

            if (draw_command.TextureId)
            {
                const auto &image_view_h = *reinterpret_cast<const vulkan::ImageViewH *>(&draw_command.TextureId);
                external_images.push_back(api.get_image_view(image_view_h).image_h);
            }
        }
    }

    graph.add_pass({
        .name              = "ImGui pass",
        .type              = PassType::Graphics,
        .external_images   = external_images,
        .color_attachments = {output},
        .exec =
            [=](RenderGraph & /*graph*/, RenderPass & /*self*/, vulkan::API &api) {
                bool success = api.start_present();
                if (!success) {
                    fmt::print(stderr, "ERROR\n");
                    return;
                }

                ImDrawData *data = ImGui::GetDrawData();

                /// --- Prepare index and vertex buffer
                auto v_pos = api.dynamic_vertex_buffer(sizeof(ImDrawVert) * static_cast<u32>(data->TotalVtxCount));
                auto i_pos = api.dynamic_index_buffer(sizeof(ImDrawIdx) * static_cast<u32>(data->TotalIdxCount));

                auto *vertices = reinterpret_cast<ImDrawVert *>(v_pos.mapped);
                auto *indices  = reinterpret_cast<ImDrawIdx *>(i_pos.mapped);

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
                }

                float4 scale_and_translation;
                scale_and_translation.raw[0] = 2.0f / data->DisplaySize.x; // X Scale
                scale_and_translation.raw[1] = 2.0f / data->DisplaySize.y; // Y Scale
                scale_and_translation.raw[2] = -1.0f - data->DisplayPos.x * scale_and_translation.raw[0]; // X Translation
                scale_and_translation.raw[3] = -1.0f - data->DisplayPos.y * scale_and_translation.raw[1]; // Y Translation

                // Will project scissor/clipping rectangles into framebuffer space
                ImVec2 clip_off   = data->DisplayPos;       // (0,0) unless using multi-viewports
                ImVec2 clip_scale = data->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

                VkViewport viewport{};
                viewport.width    = data->DisplaySize.x * data->FramebufferScale.x;
                viewport.height   = data->DisplaySize.y * data->FramebufferScale.y;
                viewport.minDepth = 1.0f;
                viewport.maxDepth = 1.0f;
                api.set_viewport(viewport);

                api.bind_vertex_buffer(v_pos);
                api.bind_index_buffer(i_pos);

                /// --- Draws

                enum UIProgram
                {
                    Float,
                    Uint
                };

                // Render GUI
                i32 vertex_offset = 0;
                u32 index_offset  = 0;
                for (int list = 0; list < data->CmdListsCount; list++)
                {
                    const auto &cmd_list = *data->CmdLists[list];

                    for (int command_index = 0; command_index < cmd_list.CmdBuffer.Size; command_index++)
                    {
                        const auto &draw_command = cmd_list.CmdBuffer[command_index];

                        vulkan::GraphicsProgramH current = pass_data.float_program;

                        if (draw_command.TextureId)
                        {
                            const auto &texture
                                = *reinterpret_cast<const vulkan::ImageViewH *>(&draw_command.TextureId);
                            auto &image_view = api.get_image_view(texture);

                            if (image_view.format == VK_FORMAT_R32_UINT)
                            {
                                current = pass_data.uint_program;
                            }

                            api.bind_combined_image_sampler(current,
                                                            texture,
                                                            api.trilinear_sampler,
                                                            vulkan::SHADER_DESCRIPTOR_SET,
                                                            0);
                        }
                        else
                        {
                            api.bind_combined_image_sampler(current,
                                                            pass_data.font_atlas,
                                                            api.trilinear_sampler,
                                                            vulkan::SHADER_DESCRIPTOR_SET,
                                                            0);
                        }

                        api.bind_program(current);
                        api.push_constant(VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float4), &scale_and_translation);

                        // Project scissor/clipping rectangles into framebuffer space
                        ImVec4 clip_rect;
                        clip_rect.x = (draw_command.ClipRect.x - clip_off.x) * clip_scale.x;
                        clip_rect.y = (draw_command.ClipRect.y - clip_off.y) * clip_scale.y;
                        clip_rect.z = (draw_command.ClipRect.z - clip_off.x) * clip_scale.x;
                        clip_rect.w = (draw_command.ClipRect.w - clip_off.y) * clip_scale.y;

                        // Apply scissor/clipping rectangle
                        // FIXME: We could clamp width/height based on clamped min/max values.
                        VkRect2D scissor;
                        scissor.offset.x      = (static_cast<i32>(clip_rect.x) > 0) ? static_cast<i32>(clip_rect.x) : 0;
                        scissor.offset.y      = (static_cast<i32>(clip_rect.y) > 0) ? static_cast<i32>(clip_rect.y) : 0;
                        scissor.extent.width  = static_cast<u32>(clip_rect.z - clip_rect.x);
                        scissor.extent.height = static_cast<u32>(clip_rect.w - clip_rect.y + 1); // FIXME: Why +1 here?

                        api.set_scissor(scissor);

                        api.draw_indexed(draw_command.ElemCount, 1, index_offset, vertex_offset, 0);

                        index_offset += draw_command.ElemCount;
                    }
                    vertex_offset += cmd_list.VtxBuffer.Size;
                }
            },
    });
}

}
