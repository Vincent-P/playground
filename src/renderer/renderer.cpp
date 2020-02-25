#include "renderer/renderer.hpp"
#include <iostream>
#include <imgui.h>

namespace my_app
{
    Renderer Renderer::create(const Window& window)
    {
        Renderer r;
        r.api = vulkan::API::create(window);

        vulkan::RTInfo info;
        info.is_swapchain = true;
        r.rt = r.api.create_rendertarget(info);

        /// --- Init ImGui

        ImGui::CreateContext();
        auto& style = ImGui::GetStyle();
        style.FrameRounding = 0.f;
        style.GrabRounding = 0.f;
        style.WindowRounding = 0.f;
        style.ScrollbarRounding = 0.f;
        style.GrabRounding = 0.f;
        style.TabRounding = 0.f;


        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize.x = float(r.api.ctx.swapchain.extent.width);
        io.DisplaySize.y = float(r.api.ctx.swapchain.extent.height);

#if 0
        vulkan::ProgramInfo pinfo;
        pinfo.v_shader = r.api.create_shader("build/shaders/gui.vert.spv");
        pinfo.f_shader = r.api.create_shader("build/shaders/gui.frag.spv");
        pinfo.push_constants({.stage = vk::ShaderStageFlagBits::eVertex, .offset = 0, .size = 4 * sizeof(float)});
        pinfo.binding(0, {.stage = vk::ShaderStageFlagBits::eFragment, .type = vk::DescriptorType::eCombinedImageSampler, .count = 1} );
        r.gui_program = r.api.create_program(pinfo);
#endif

        vulkan::ImageInfo iinfo;
        iinfo.name = "ImGui texture";

        uchar* pixels = nullptr;

        // Get image data
        int w = 0, h = 0;
        ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);

        iinfo.width = static_cast<u32>(w);
        iinfo.height = static_cast<u32>(h);
        iinfo.depth = 1;

        r.gui_texture = r.api.create_image(iinfo);
        r.api.upload_image(r.gui_texture, pixels, iinfo.width * iinfo.height * 4);

        return r;
    }

    void Renderer::destroy()
    {
        api.destroy_image(gui_texture);
        api.destroy();
    }

    void Renderer::on_resize(int width, int height)
    {
        api.on_resize(width, height);
    }

    void Renderer::wait_idle()
    {
        api.wait_idle();
    }

    void Renderer::draw()
    {
        api.start_frame();

        vulkan::PassInfo pass;
        pass.clear = true;
        pass.present = true;
        pass.rt = rt;

        api.begin_pass(pass);

#if 0
        api.bind_program(gui_program);
        // make dynamic vertex buffer
        // make dynamic index buffer
        // push constants
        // bind texture (should be done in create)
        // foreach (imguicommand) draw
#endif

        api.end_pass();

        api.end_frame();
    }
}
