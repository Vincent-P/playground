#include "ui.h"

#include <exo/os/window.h>
#include <exo/collections/map.h>

#include "inputs.h"

#include <imgui.h>
#include <fmt/format.h>
#include <string>

namespace
{
static exo::Cursor cursor_from_imgui()
{
    ImGuiIO         &io           = ImGui::GetIO();
    ImGuiMouseCursor imgui_cursor = io.MouseDrawCursor ? ImGuiMouseCursor_None : ImGui::GetMouseCursor();
    exo::Cursor    cursor       = exo::Cursor::None;
    switch (imgui_cursor)
    {
    case ImGuiMouseCursor_Arrow:
        cursor = exo::Cursor::Arrow;
        break;
    case ImGuiMouseCursor_TextInput:
        cursor = exo::Cursor::TextInput;
        break;
    case ImGuiMouseCursor_ResizeAll:
        cursor = exo::Cursor::ResizeAll;
        break;
    case ImGuiMouseCursor_ResizeEW:
        cursor = exo::Cursor::ResizeEW;
        break;
    case ImGuiMouseCursor_ResizeNS:
        cursor = exo::Cursor::ResizeNS;
        break;
    case ImGuiMouseCursor_ResizeNESW:
        cursor = exo::Cursor::ResizeNESW;
        break;
    case ImGuiMouseCursor_ResizeNWSE:
        cursor = exo::Cursor::ResizeNWSE;
        break;
    case ImGuiMouseCursor_Hand:
        cursor = exo::Cursor::Hand;
        break;
    case ImGuiMouseCursor_NotAllowed:
        cursor = exo::Cursor::NotAllowed;
        break;
    }
    return cursor;
}
} // namespace

namespace UI
{
struct Window
{
    const char *name = "no name";
    bool is_opened = false;
};

struct Context
{
    std::string ini_path = {};
    exo::Map<std::string_view, Window> windows = {};
    exo::Window *window;
    Inputs *inputs;
};

Context* g_ui_context = nullptr;

void create_context(exo::Window *window, Inputs *inputs)
{
    ASSERT(g_ui_context == nullptr);
    g_ui_context = new Context();
    g_ui_context->ini_path = fmt::format("{}/{}", ASSET_PATH, "imgui.ini");
    g_ui_context->window = window;
    g_ui_context->inputs = inputs;

    // Init ImGui
    ImGui::CreateContext();
    auto &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigDockingWithShift = false;
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors; // We can honor GetMouseCursor() values (optional)
    io.BackendPlatformName = "custom";
    io.IniFilename         = g_ui_context->ini_path.c_str();
    io.Fonts->AddFontDefault();
}

void destroy_context(Context *context)
{
    bool is_global = context == nullptr;
    if (is_global)
    {
        context = g_ui_context;
    }
    ASSERT(context != nullptr);
    delete context;

    ImGui::DestroyContext();
}

void display_ui()
{
    auto &ctx = *g_ui_context;

    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("Windows"))
        {
            for (auto &[_, window] : ctx.windows)
            {
                ImGui::PushID(&window);
                ImGui::MenuItem(window.name, nullptr, &window.is_opened);
                ImGui::PopID();
            }

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}

void new_frame()
{
    auto *window = g_ui_context->window;
    auto *inputs = g_ui_context->inputs;

    ImGuiIO &io                  = ImGui::GetIO();
    io.DisplaySize.x             = float(window->width);
    io.DisplaySize.y             = float(window->height);
    io.DisplayFramebufferScale.x = window->get_dpi_scale().x;
    io.DisplayFramebufferScale.y = window->get_dpi_scale().y;

    io.MousePos = window->mouse_position;

    if (auto scroll = inputs->get_scroll_this_frame())
    {
        io.MouseWheel += static_cast<float>(-scroll->y);
    }

    if (io.WantCaptureMouse)
    {
        inputs->consume_scroll();
    }

    static_assert(static_cast<usize>(exo::MouseButton::Count) == 5u);
    for (uint i = 0; i < 5; i++)
    {
        io.MouseDown[i] = inputs->is_pressed(static_cast<exo::MouseButton>(i));
    }

    auto cursor        = cursor_from_imgui();
    static auto s_last = cursor;
    if (s_last != cursor)
    {
        window->set_cursor(cursor);
        s_last = cursor;
    }

    // NewFrame() has to be called before giving the inputs to imgui
    ImGui::NewFrame();
}


WindowImpl::WindowImpl(const char *name, bool *p_open, ImGuiWindowFlags flags)
{
    this->is_opened = *p_open;
    this->is_visible = false;

    if (this->is_opened)
    {
        this->is_visible = ImGui::Begin(name, p_open, flags);
    }
}

WindowImpl::~WindowImpl()
{
    if (this->is_opened)
    {
        ImGui::End();
    }
}

WindowImpl begin_window(const char *name, ImGuiWindowFlags flags)
{
    auto &ctx = *g_ui_context;

    if (!ctx.windows.contains(name))
    {
        ctx.windows[name] = {.name = name, .is_opened = false};
    }
    auto &window = ctx.windows.at(name);

    return WindowImpl(name, &window.is_opened, flags);
}
}
