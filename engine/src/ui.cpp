#include "ui.h"

#include "inputs.h"
#include <cross/window.h>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

namespace UI
{
Context Context::create()
{
    Context ctx = {};
    // Init context
    ImGui::CreateContext();

    auto &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigDockingWithShift = false;
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors; // We can honor GetMouseCursor() values (optional)
    io.BackendPlatformName = "custom";

    // Add fonts
    io.Fonts->AddFontDefault();
    return ctx;
}

static cross::Cursor cursor_from_imgui()
{
    ImGuiIO &io                   = ImGui::GetIO();
    ImGuiMouseCursor imgui_cursor = io.MouseDrawCursor ? ImGuiMouseCursor_None : ImGui::GetMouseCursor();
    cross::Cursor cursor       = cross::Cursor::None;
    switch (imgui_cursor)
    {
        case ImGuiMouseCursor_Arrow:
            cursor = cross::Cursor::Arrow;
            break;
        case ImGuiMouseCursor_TextInput:
            cursor = cross::Cursor::TextInput;
            break;
        case ImGuiMouseCursor_ResizeAll:
            cursor = cross::Cursor::ResizeAll;
            break;
        case ImGuiMouseCursor_ResizeEW:
            cursor = cross::Cursor::ResizeEW;
            break;
        case ImGuiMouseCursor_ResizeNS:
            cursor = cross::Cursor::ResizeNS;
            break;
        case ImGuiMouseCursor_ResizeNESW:
            cursor = cross::Cursor::ResizeNESW;
            break;
        case ImGuiMouseCursor_ResizeNWSE:
            cursor = cross::Cursor::ResizeNWSE;
            break;
        case ImGuiMouseCursor_Hand:
            cursor = cross::Cursor::Hand;
            break;
        case ImGuiMouseCursor_NotAllowed:
            cursor = cross::Cursor::NotAllowed;
            break;
    }
    return cursor;
}

void Context::on_mouse_movement(cross::Window &window, double /*xpos*/, double /*ypos*/)
{
    auto cursor = cursor_from_imgui();
    window.set_cursor(cursor);
}

void Context::start_frame(cross::Window &window, Inputs &inputs)
{
    ImGuiIO &io                  = ImGui::GetIO();
    io.DisplaySize.x             = float(window.width);
    io.DisplaySize.y             = float(window.height);
    io.DisplayFramebufferScale.x = window.get_dpi_scale().x;
    io.DisplayFramebufferScale.y = window.get_dpi_scale().y;

    io.MousePos = window.mouse_position;

    if (auto scroll = inputs.get_scroll_this_frame())
    {
        io.MouseWheel += static_cast<float>(-scroll->y);
    }

    if (io.WantCaptureMouse)
    {
        inputs.scroll_this_frame = {};
    }

    static_assert(static_cast<usize>(MouseButton::Count) == 5u);
    for (uint i = 0; i < 5; i++)
    {
        io.MouseDown[i] = inputs.is_pressed(static_cast<MouseButton>(i));
    }

    auto cursor        = cursor_from_imgui();
    static auto s_last = cursor;
    if (s_last != cursor)
    {
        window.set_cursor(cursor);
        s_last = cursor;
    }

    // NewFrame() has to be called before giving the inputs to imgui
    ImGui::NewFrame();
}

void Context::destroy() { ImGui::DestroyContext(); }

void Context::display_ui()
{
    ZoneScoped;
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("Windows"))
        {
            for (auto &[_, window] : windows)
            {
                ImGui::MenuItem(window.name.c_str(), nullptr, &window.is_visible);
            }

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}

bool Context::begin_window(std::string_view name, bool is_visible, ImGuiWindowFlags /*flags*/)
{
    if (!windows.contains(name))
    {
        windows[name] = {.name = std::string(name), .is_visible = is_visible};
    }

    auto &window = windows.at(name);
    if (window.is_visible)
    {
        ImGui::Begin(name.data(), &window.is_visible, 0);
        return true;
    }

    return false;
}

void Context::end_window() { ImGui::End(); }
}
