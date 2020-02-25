#include "app.hpp"
#include <imgui.h>

namespace my_app
{
    constexpr auto DEFAULT_WIDTH = 1280;
    constexpr auto DEFAULT_HEIGHT = 720;

    App::App()
	    : window(DEFAULT_WIDTH, DEFAULT_HEIGHT)
    {
	renderer = Renderer::create(window);

	window.register_resize_callback([this](int width, int height) {
	    this->renderer.on_resize(width, height);
	});
    }

    App::~App()
    {
	renderer.destroy();
    }

    void App::run()
    {
	while (!window.should_close())
	{
            ImGui::NewFrame();
	    window.update();
	    renderer.draw();
	}

	renderer.wait_idle();
    }
}    // namespace my_app
