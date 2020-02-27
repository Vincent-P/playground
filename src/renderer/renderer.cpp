#include "renderer/renderer.hpp"
#include <imgui.h>
#include <iostream>
#include "window.hpp"

namespace my_app
{

Renderer Renderer::create(const Window &window)
{
    Renderer r;
    r.p_window = &window;
    r.api = vulkan::API::create(window);

    vulkan::RTInfo info;
    info.is_swapchain = true;
    r.rt              = r.api.create_rendertarget(info);

    /// --- Init ImGui
    ImGui::CreateContext();
    auto &style             = ImGui::GetStyle();
    style.FrameRounding     = 0.f;
    style.GrabRounding      = 0.f;
    style.WindowRounding    = 0.f;
    style.ScrollbarRounding = 0.f;
    style.GrabRounding      = 0.f;
    style.TabRounding       = 0.f;

    ImGuiIO &io      = ImGui::GetIO();

    io.DisplaySize.x = float(r.api.ctx.swapchain.extent.width);
    io.DisplaySize.y = float(r.api.ctx.swapchain.extent.height);
    io.DisplayFramebufferScale.x = window.get_dpi_scale().x;
    io.DisplayFramebufferScale.y = window.get_dpi_scale().y;

    vulkan::ProgramInfo pinfo{};
    pinfo.vertex_shader   = r.api.create_shader("shaders/gui.vert.spv");
    pinfo.fragment_shader = r.api.create_shader("shaders/gui.frag.spv");
    pinfo.push_constant({/*.stages = */ vk::ShaderStageFlagBits::eVertex, /*.offset = */ 0, /*.size = */ 4 * sizeof(float)});
    pinfo.binding({/*.slot = */ 0, /*.stages = */ vk::ShaderStageFlagBits::eFragment,/*.type = */ vk::DescriptorType::eCombinedImageSampler, /*.count = */ 1});
    pinfo.vertex_stride(sizeof(ImDrawVert));

    pinfo.vertex_info({vk::Format::eR32G32Sfloat, MEMBER_OFFSET(ImDrawVert, pos)});
    pinfo.vertex_info({vk::Format::eR32G32Sfloat, MEMBER_OFFSET(ImDrawVert, uv)});
    pinfo.vertex_info({vk::Format::eR8G8B8A8Unorm, MEMBER_OFFSET(ImDrawVert, col)});

    r.gui_program = r.api.create_program(std::move(pinfo));

    vulkan::ImageInfo iinfo;
    iinfo.name = "ImGui texture";

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

    return r;
}

void Renderer::destroy()
{
    api.destroy_image(gui_texture);
    api.destroy();
}

void Renderer::on_resize(int width, int height) { api.on_resize(width, height); }

void Renderer::wait_idle() { api.wait_idle(); }

void Renderer::imgui_draw()
{
    vulkan::PassInfo pass;
    pass.present            = true;
    pass.attachment.load_op = vk::AttachmentLoadOp::eClear;
    pass.attachment.rt      = rt;

    ImGui::Render();

    ImDrawData *data = ImGui::GetDrawData();
    if (data != nullptr && data->TotalVtxCount != 0) {

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

	auto dpi_scale = p_window->get_dpi_scale();
        vk::Viewport viewport{};
        viewport.width    = data->DisplaySize.x * data->FramebufferScale.x;
        viewport.height   = data->DisplaySize.y * data->FramebufferScale.y;
        viewport.minDepth = 1.0f;
        viewport.maxDepth = 1.0f;
        api.set_viewport(viewport);

        api.begin_pass(std::move(pass));
        api.bind_image(gui_program, 0, gui_texture);
        api.bind_program(gui_program);
        api.bind_vertex_buffer(v_pos);
        api.bind_index_buffer(i_pos);

	float4 scale_and_translation;
	scale_and_translation[0] = 2.0f / data->DisplaySize.x; // X Scale
	scale_and_translation[1] = 2.0f / data->DisplaySize.y; // Y Scale
	scale_and_translation[2] = -1.0f - data->DisplayPos.x * scale_and_translation[0]; // X Translation
	scale_and_translation[3] = -1.0f - data->DisplayPos.y * scale_and_translation[1]; // Y Translation

        api.push_constant(vk::ShaderStageFlagBits::eVertex, 0, sizeof(float4), &scale_and_translation);

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

                // Project scissor/clipping rectangles into framebuffer space
                ImVec4 clip_rect;
                clip_rect.x = (draw_command->ClipRect.x - clip_off.x) * clip_scale.x;
                clip_rect.y = (draw_command->ClipRect.y - clip_off.y) * clip_scale.y;
                clip_rect.z = (draw_command->ClipRect.z - clip_off.x) * clip_scale.x;
                clip_rect.w = (draw_command->ClipRect.w - clip_off.y) * clip_scale.y;

                // Apply scissor/clipping rectangle
                // FIXME: We could clamp width/height based on clamped min/max values.
		vk::Rect2D scissor;
                scissor.offset.x = ((int32_t)clip_rect.x > 0) ? (int32_t)(clip_rect.x) : 0;
                scissor.offset.y = ((int32_t)clip_rect.y > 0) ? (int32_t)(clip_rect.y) : 0;
                scissor.extent.width = (uint32_t)(clip_rect.z - clip_rect.x);
                scissor.extent.height = (uint32_t)(clip_rect.w - clip_rect.y + 1); // FIXME: Why +1 here?

                api.set_scissor(scissor);

                api.draw_indexed(draw_command->ElemCount, 1, index_offset, vertex_offset, 0);

                index_offset += draw_command->ElemCount;
            }
            vertex_offset += cmd_list->VtxBuffer.Size;
        }
    }
    else {
        api.begin_pass(std::move(pass));
    }

    api.end_pass();
}

void Renderer::draw()
{
    bool is_ok = api.start_frame();
    if (is_ok) {
        imgui_draw();
        api.end_frame();
    }
    else {
	ImGui::EndFrame();
    }
}

} // namespace my_app
