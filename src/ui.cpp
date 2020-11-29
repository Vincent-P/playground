#include "ui.hpp"

#include "base/types.hpp"
#include "platform/window.hpp"

#include <imgui/imgui.h>
#include <iostream>

namespace my_app
{
namespace UI
{

void Context::create(Context & /*ctx*/)
{
    // Init context
    ImGui::CreateContext();

    auto &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigDockingWithShift = false;
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors; // We can honor GetMouseCursor() values (optional)
    io.BackendPlatformName = "custom";

    // Add fonts
    io.Fonts->AddFontDefault();
    ImFontConfig config;
    config.MergeMode                                = true;
    config.GlyphMinAdvanceX                         = 13.0f; // Use if you want to make the icon monospaced
    static const std::array<ImWchar, 3> icon_ranges = {eva_icons::MIN, eva_icons::MAX, 0};
    io.Fonts->AddFontFromFileTTF("../fonts/Eva-Icons.ttf", 13.0f, &config, icon_ranges.data());
}

static window::Cursor cursor_from_imgui()
{
    ImGuiIO &io                   = ImGui::GetIO();
    ImGuiMouseCursor imgui_cursor = io.MouseDrawCursor ? ImGuiMouseCursor_None : ImGui::GetMouseCursor();
    window::Cursor cursor         = window::Cursor::None;
    switch (imgui_cursor)
    {
        case ImGuiMouseCursor_Arrow:
            cursor = window::Cursor::Arrow;
            break;
        case ImGuiMouseCursor_TextInput:
            cursor = window::Cursor::TextInput;
            break;
        case ImGuiMouseCursor_ResizeAll:
            cursor = window::Cursor::ResizeAll;
            break;
        case ImGuiMouseCursor_ResizeEW:
            cursor = window::Cursor::ResizeEW;
            break;
        case ImGuiMouseCursor_ResizeNS:
            cursor = window::Cursor::ResizeNS;
            break;
        case ImGuiMouseCursor_ResizeNESW:
            cursor = window::Cursor::ResizeNESW;
            break;
        case ImGuiMouseCursor_ResizeNWSE:
            cursor = window::Cursor::ResizeNWSE;
            break;
        case ImGuiMouseCursor_Hand:
            cursor = window::Cursor::Hand;
            break;
        case ImGuiMouseCursor_NotAllowed:
            cursor = window::Cursor::NotAllowed;
            break;
    }
    return cursor;
}

void Context::on_mouse_movement(::window::Window &window, double /*xpos*/, double /*ypos*/)
{
    auto cursor = cursor_from_imgui();
    window.set_cursor(cursor);
}

void Context::start_frame(::window::Window &window)
{
    ImGuiIO &io                  = ImGui::GetIO();
    io.DisplaySize.x             = window.width;
    io.DisplaySize.y             = window.height;
    io.DisplayFramebufferScale.x = window.get_dpi_scale().x;
    io.DisplayFramebufferScale.y = window.get_dpi_scale().y;

    io.MousePos = window.mouse_position;

    static_assert(to_underlying(window::MouseButton::Count) == 5);
    for (uint i = 0; i < 5; i++)
    {
        io.MouseDown[i] = window.mouse_buttons_pressed[i];
    }

    auto cursor = cursor_from_imgui();
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
    // const auto &io = ImGui::GetIO();
    return;

    if (ImGui::BeginMainMenuBar())
    {
        if (!windows.empty())
        {
            ImGui::TextUnformatted("|");
        }
        for (auto &[_, window] : windows)
        {
            if (window.is_visible)
            {
                ImGui::TextUnformatted("o");
            }
            ImGui::MenuItem(window.name.c_str(), nullptr, &window.is_visible);
            ImGui::TextUnformatted("|");
        }

        ImGui::EndMainMenuBar();
  }
}

bool Context::begin_window(std::string_view name, bool is_visible, ImGuiWindowFlags flags)
{
    if (windows.count(name))
    {
        auto &window = windows.at(name);
        if (true || window.is_visible)
        {
            ImGui::Begin(name.data(), &window.is_visible, flags);
            return true;
        }
    }
    else
    {
        windows[name] = {.name = std::string(name), .is_visible = is_visible};
    }

    return false;
}

void Context::end_window() { ImGui::End(); }
} // namespace UI
} // namespace my_app
