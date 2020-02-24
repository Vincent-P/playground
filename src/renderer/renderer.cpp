#include "renderer/renderer.hpp"
#include <iostream>

namespace my_app
{
    Renderer Renderer::create(const Window& window)
    {
	Renderer r;
	r.api = vulkan::API::create(window);

	vulkan::RTInfo info;
	info.is_swapchain = true;
	r.rt = r.api.create_rendertarget(info);

	return r;
    }

    void Renderer::destroy()
    {
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
	api.end_pass();

	api.end_frame();
    }
}
