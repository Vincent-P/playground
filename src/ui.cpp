#include "ui.hpp"

#include "base/types.hpp"
#include "platform/window.hpp"

#include <iostream>

namespace my_app
{
namespace UI
{

void Context::create(Context &/*ctx*/)
{
    // Init context
    ImGui::CreateContext();

    auto &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigDockingWithShift = false;
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;         // We can honor GetMouseCursor() values (optional)
    io.BackendPlatformName = "custom";

    // Add fonts
    io.Fonts->AddFontDefault();
    ImFontConfig config;
    config.MergeMode                                = true;
    config.GlyphMinAdvanceX                         = 13.0f; // Use if you want to make the icon monospaced
    static const std::array<ImWchar, 3> icon_ranges = {eva_icons::MIN, eva_icons::MAX, 0};
    io.Fonts->AddFontFromFileTTF("../fonts/Eva-Icons.ttf", 13.0f, &config, icon_ranges.data());
}

void Context::start_frame(::window::Window &window)
{
    ImGuiIO &io  = ImGui::GetIO();
    io.DisplaySize.x             = window.width;
    io.DisplaySize.y             = window.height;
    io.DisplayFramebufferScale.x = window.get_dpi_scale().x;
    io.DisplayFramebufferScale.y = window.get_dpi_scale().y;

    // NewFrame() has to be called before giving the inputs to imgui
    ImGui::NewFrame();

    io.MousePos = window.mouse_position;

    static_assert(to_underlying(window::MouseButton::Count) == 5);
    for (uint i = 0; i < 5; i++) {
        io.MouseDown[i] = window.mouse_buttons_pressed[i];
    }

    if ((io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange) == 0)
    {
        ImGuiMouseCursor imgui_cursor = ImGui::GetMouseCursor();
        window::Cursor cursor = window::Cursor::None;
        if (!io.MouseDrawCursor)
        {
            switch (imgui_cursor)
            {
            case ImGuiMouseCursor_Arrow:        cursor = window::Cursor::Arrow; break;
            case ImGuiMouseCursor_TextInput:    cursor = window::Cursor::TextInput; break;
            case ImGuiMouseCursor_ResizeAll:    cursor = window::Cursor::ResizeAll; break;
            case ImGuiMouseCursor_ResizeEW:     cursor = window::Cursor::ResizeEW; break;
            case ImGuiMouseCursor_ResizeNS:     cursor = window::Cursor::ResizeNS; break;
            case ImGuiMouseCursor_ResizeNESW:   cursor = window::Cursor::ResizeNESW; break;
            case ImGuiMouseCursor_ResizeNWSE:   cursor = window::Cursor::ResizeNWSE; break;
            case ImGuiMouseCursor_Hand:         cursor = window::Cursor::Hand; break;
            case ImGuiMouseCursor_NotAllowed:   cursor = window::Cursor::NotAllowed; break;
            }
        }
        window.set_cursor(cursor);
    }
}

void Context::destroy()
{
    ImGui::DestroyContext();
}

void Context::display_ui()
{
    const auto &io = ImGui::GetIO();
    if (ImGui::BeginMainMenuBar())
    {
        auto display_width = io.DisplaySize.x * io.DisplayFramebufferScale.x;
        ImGui::Dummy(ImVec2(display_width / 2 - 100.0f * io.DisplayFramebufferScale.x, 0.0f));

        ImGui::TextUnformatted("|");
        if (ImGui::BeginMenu(EVA_MENU " Menu"))
        {
            for (auto &[_, window] : windows)
            {
                ImGui::MenuItem(window.name.c_str(), nullptr, &window.is_visible);
            }
            ImGui::EndMenu();
        }

        for (auto &[_, window] : windows)
        {
            if (window.is_visible)
            {
                ImGui::TextUnformatted("|");
                ImGui::MenuItem(window.name.c_str(), nullptr, &window.is_visible);
            }
        }
        ImGui::TextUnformatted("|");

        ImGui::EndMainMenuBar();
    }
}

bool Context::begin_window(std::string_view name, bool is_visible, ImGuiWindowFlags flags)
{
    if (windows.count(name))
    {
        auto &window = windows.at(name);
        if (window.is_visible)
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

void Context::end_window()
{
    ImGui::End();
}
} // namespace UI
} // namespace my_app
