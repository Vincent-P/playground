#include "app.hpp"

#include "file_watcher.hpp"

#include <algorithm>
#include <imgui/imgui.h>
#include <iostream>
#include <sstream>
#include <variant>

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
        if (watch.wd != shaders_watch.wd)
        {
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
    window.destroy();
}

void App::update() { camera.update(); }

void App::display_ui()
{
    ui.start_frame(window);
    ui.display_ui();
    camera.display_ui(ui);
    renderer.display_ui(ui);
    ecs.display_ui(ui);

    constexpr float fov  = 60.f;
    constexpr float size = 50.f;
    const ImGuiCol red   = ImGui::GetColorU32(float4(255.f / 256.f, 56.f / 256.f, 86.f / 256.f, 1.0f));
    const ImGuiCol green = ImGui::GetColorU32(float4(143.f / 256.f, 226.f / 256.f, 10.f / 256.f, 1.0f));
    const ImGuiCol blue  = ImGui::GetColorU32(float4(52.f / 256.f, 146.f / 256.f, 246.f / 256.f, 1.0f));
    const ImGuiCol black = ImGui::GetColorU32(float4(0.0f, 0.0f, 0.0f, 1.0f));

    float3 camera_forward  = normalize(camera.target - camera._internal.position);
    float3 origin          = float3(0.f);
    float3 camera_position = origin - 2.0f * camera_forward;
    auto view              = Camera::look_at(camera_position, origin, camera._internal.up);
    auto proj              = Camera::perspective(fov, 1.f, 0.01f, 10.0f);

    struct GizmoAxis
    {
        const char *label;
        float3 axis;
        float2 projected_point;
        ImGuiCol color;
        bool draw_line;
    };

    std::vector<GizmoAxis> axes{
        {.label = "X", .axis = float3(1.0f, 0.0f, 0.0f), .color = red, .draw_line = true},
        {.label = "Y", .axis = float3(0.0f, 1.0f, 0.0f), .color = green, .draw_line = true},
        {.label = "Z", .axis = float3(0.0f, 0.0f, 1.0f), .color = blue, .draw_line = true},
        {.label = "-X", .axis = float3(-1.0f, 0.0f, 0.0f), .color = red, .draw_line = false},
        {.label = "-Y", .axis = float3(0.0f, -1.0f, 0.0f), .color = green, .draw_line = false},
        {.label = "-Z", .axis = float3(0.0f, 0.0f, -1.0f), .color = blue, .draw_line = false},
    };

    for (auto &axis : axes)
    {
        // project 3d point to 2d
        float4 projected_p = proj * view * float4(axis.axis, 1.0f);
        projected_p        = (1.0f / projected_p.w) * projected_p;

        // remap [-1, 1] to [-0.9 * size, 0.9 * size] to fit the canvas
        axis.projected_point = 0.9f * size * projected_p.xy();
    }

    // sort by distance to the camera
    std::sort(std::begin(axes), std::end(axes), [&](const GizmoAxis &a, const GizmoAxis &b) {
        return (camera_position - a.axis).squared_norm() > (camera_position - b.axis).squared_norm();
    });

    auto flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoResize
                 | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar;
    ImGui::Begin("Gizmo", nullptr, flags);

    ImDrawList *draw_list = ImGui::GetWindowDrawList();
    float2 p              = ImGui::GetCursorScreenPos();

    // ceenter p
    p = p + float2(size);

    auto font_size   = ImGui::GetFontSize();
    float2 half_size = float2(font_size / 2.f);
    half_size.x /= 2;

    // draw each axis
    for (const auto &axis : axes)
    {
        if (axis.draw_line)
        {
            draw_list->AddLine(p, p + axis.projected_point, axis.color, 3.0f);
        }

        draw_list->AddCircleFilled(p + axis.projected_point, 7.f, axis.color);

        if (axis.draw_line && axis.label)
        {
            draw_list->AddText(p + axis.projected_point - half_size, black, axis.label);
        }
    }

    ImGui::Dummy(float2(2 * size));

    ImGui::End();
}

void App::run()
{
    while (!window.should_close())
    {
        window.poll_events();

        std::optional<window::event::Resize> last_resize;
        for (auto &event : window.events)
        {
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
                if (key.key == window::VirtualKey::Escape)
                {
                    window.stop = true;
                }
            }
        }

        if (last_resize)
        {
            auto resize = *last_resize;
            if (resize.width > 0 && resize.height > 0)
            {
                renderer.on_resize(resize.width, resize.height);
            }
            if (window.minimized)
            {
                this->is_minimized = true;
            }
        }

        window.events.clear();

        if (is_minimized)
        {
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
