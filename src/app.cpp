#include "app.hpp"
#include <algorithm>
#include <variant>
#include <imgui/imgui.h>
#include <iostream>
#include <sstream>
#include "file_watcher.hpp"

namespace my_app
{

constexpr auto DEFAULT_WIDTH  = 1920;
constexpr auto DEFAULT_HEIGHT = 1080;

App::App()
{
    window::Window::create(window, DEFAULT_WIDTH, DEFAULT_HEIGHT, "Test vulkan");
    UI::Context::create(ui);

    InputCamera::create(camera, window, timer, float3(4.0f, 14.5f, 0.0f));
    camera._internal.yaw   = 90.0f;
    camera._internal.pitch = 0.0f;

    Renderer::create(renderer, window, camera._internal, timer, ui);

    watcher = FileWatcher::create();

    shaders_watch = watcher.add_watch("shaders");

    watcher.on_file_change([&](const auto &watch, const auto &event) {
        if (watch.wd != shaders_watch.wd) {
            return;
        }


        std::stringstream shader_name_stream;
        shader_name_stream << "shaders/" << event.name;
        std::string shader_name = shader_name_stream.str();

        this->renderer.reload_shader(shader_name);
    });

    is_minimized = false;
}

App::~App()
{
    ui.destroy();
    renderer.destroy();
}

void App::update()
{
    camera.update();
}

void App::display_ui()
{
    ui.start_frame(window);
    ui.display_ui();
    camera.display_ui(ui);
    renderer.display_ui(ui);

    auto flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar;
    ImGui::Begin("Gizmo", nullptr, flags);

    float s_fov = 60.f;
    float s_size = 50.f;

    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    const ImGuiCol red   = ImGui::GetColorU32(float4(255.f/256.f, 56.f/256.f, 86.f/256.f, 1.0f));
    const ImGuiCol green = ImGui::GetColorU32(float4(143.f/256.f, 226.f/256.f, 10.f/256.f, 1.0f));
    const ImGuiCol blue  = ImGui::GetColorU32(float4(52.f/256.f, 146.f/256.f, 246.f/256.f, 1.0f));
    const ImGuiCol white  = ImGui::GetColorU32(float4(1.0f));
    const ImGuiCol black  = ImGui::GetColorU32(float4(0.0f, 0.0f, 0.0f, 1.0f));

    float3 camera_forward = normalize(camera.target - camera._internal.position);

    auto view = Camera::look_at(float3(0.0f) - 2.0f * camera_forward, float3(0.0f), camera._internal.up);
    auto proj = Camera::perspective(s_fov, 1.f, 0.01f, 10.0f);

    auto project_point = [&](float3 p) {
        float4 projected_p = proj * view * float4(p, 1.0f);
        projected_p = (1.0f / projected_p.w) * projected_p;
        return projected_p.xy();
    };

    // from [-1, 1] to [-s_size, s_size]
    float2 x       = 0.9f * s_size * project_point(float3(1.0f, 0.0f, 0.0f));
    float2 y       = 0.9f * s_size * project_point(float3(0.0f, 1.0f, 0.0f));
    float2 z       = 0.9f * s_size * project_point(float3(0.0f, 0.0f, 1.0f));
    float2 minus_x = 0.9f * s_size * project_point(float3(-1.0f, 0.0f, 0.0f));
    float2 minus_y = 0.9f * s_size * project_point(float3(0.0f, -1.0f, 0.0f));
    float2 minus_z = 0.9f * s_size * project_point(float3(0.0f, 0.0f, -1.0f));

    float2 p = ImGui::GetCursorScreenPos();

    // ceenter p
    p = p + float2(s_size);

    // draw origin as white circle
    draw_list->AddCircle(p, 5.f, white);

    auto font_size = ImGui::GetFontSize();
    float2 half_size = float2(font_size/2.f);
    half_size.x /= 2;

    auto draw_axis_circle = [&](const char* label, float2 screen_point, ImGuiCol color) {
        draw_list->AddCircleFilled(p + screen_point, 7.f, color);
        if (label)
            draw_list->AddText(p + screen_point - half_size, black, label);
    };


    // draw axis as line with a circle at the end
    auto draw_axis = [&](const char* label, float2 screen_point, ImGuiCol color) {
        draw_list->AddLine(p, p + screen_point, color, 3.0f);
        draw_axis_circle(label, screen_point, color);
    };

    draw_axis("X", x, red);
    draw_axis_circle(nullptr, minus_x, red);
    draw_axis("Y", y, green);
    draw_axis_circle(nullptr, minus_y, green);
    draw_axis("Z", z, blue);
    draw_axis_circle(nullptr, minus_z, blue);

    ImGui::Dummy(float2(2 * s_size));

    ImGui::End();
}

void App::run()
{
    while (!window.should_close()) {
        window.poll_events();

        std::optional<window::event::Resize> last_resize;
        for (auto &event : window.events) {
            if (std::holds_alternative<window::event::Resize>(event))
            {
                auto resize = std::get<window::event::Resize>(event);
                last_resize = resize;
            }
            else if (std::holds_alternative<window::event::Scroll>(event))
            {
                auto scroll = std::get<window::event::Scroll>(event);
                this->camera.on_mouse_scroll(double(scroll.dx), double(scroll.dy));
            }
            else if (std::holds_alternative<window::event::MouseMove>(event))
            {
                auto move = std::get<window::event::MouseMove>(event);
                this->camera.on_mouse_movement(double(move.x), double(move.y));
                this->is_minimized = false;
            }
            else if (std::holds_alternative<window::event::Key>(event))
            {
                auto key = std::get<window::event::Key>(event);
                if (key.key == window::VirtualKey::Escape) {
                    window.stop = true;
                }
            }
        }

        if (last_resize)
        {
            auto resize = *last_resize;
            if (resize.width > 0 && resize.height > 0) {
                renderer.on_resize(resize.width, resize.height);
            }
            if (window.minimized) {
                this->is_minimized = true;
            }
        }

        window.events.clear();

        if (is_minimized) {
            continue;
        }

        update();
        timer.update();
        display_ui();
        renderer.draw();
        watcher.update();

    }

    renderer.wait_idle();
}

} // namespace my_app
