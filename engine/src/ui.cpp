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


    #if 0
    auto *viewport = ImGui::GetMainViewport();

    ImGui::SetNextWindowPos(viewport->GetWorkPos());
    ImGui::SetNextWindowSize(viewport->GetWorkSize());
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags host_window_flags = 0;
    host_window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking;
    host_window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;


    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("Fullscreen Window", nullptr, host_window_flags);
    ImGui::PopStyleVar(3);

    ImGuiID dockspace_id = ImGui::GetID("DockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f));

    static bool ran = false;
    if (!ran)
    {
        ran = true;
        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace_id, io.DisplaySize);

        float left_col_width = 0.20f;
        float left_bottom_height = 0.33f;
        float toolbar_height = 0.30f;
        float bottom_height = 0.30f;
        float right_col_width = 0.20f;
        float right_bottom_height = 0.33f;

        ImGuiID dock_main_id   = dockspace_id;
        ImGuiID dock_id_left   = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, left_col_width, nullptr, &dock_main_id);
        ImGuiID dock_id_left_bottom   = ImGui::DockBuilderSplitNode(dock_id_left, ImGuiDir_Down, left_bottom_height, nullptr, &dock_id_left);
        ImGuiID dock_id_right  = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, right_col_width, nullptr, &dock_main_id);
        ImGuiID dock_id_right_bottom   = ImGui::DockBuilderSplitNode(dock_id_right, ImGuiDir_Down, right_bottom_height, nullptr, &dock_id_right);
        ImGuiID dock_id_bottom = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, bottom_height, nullptr, &dock_main_id);
        ImGuiID dock_id_up     = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Up, toolbar_height, nullptr, &dock_main_id);
        (void)(dock_id_left);
        (void)(dock_id_left_bottom);
        (void)(dock_id_right);
        (void)(dock_id_right_bottom);
        (void)(dock_id_bottom);
        (void)(dock_id_up);

        ImGui::DockBuilderDockWindow("ECS", dock_id_left);
        ImGui::DockBuilderDockWindow("Inputs", dock_id_left);
        ImGui::DockBuilderDockWindow("Render Graph", dock_id_left);
        ImGui::DockBuilderDockWindow("Renderer", dock_id_left);
        ImGui::DockBuilderDockWindow("Scene", dock_id_left);

        ImGui::DockBuilderDockWindow("Shaders", dock_id_left_bottom);
        ImGui::DockBuilderDockWindow("Inspector", dock_id_left_bottom);

        ImGui::DockBuilderDockWindow("Profiler", dock_id_right);
        ImGui::DockBuilderDockWindow("Settings", dock_id_left_bottom);

        ImGui::DockBuilderDockWindow("Framebuffer", dock_main_id);

        ImGui::DockBuilderFinish(dockspace_id);

        if (ImGuiDockNode* node = ImGui::DockBuilderGetCentralNode(dockspace_id)) {
            node->LocalFlags |= ImGuiDockNodeFlags_NoTabBar | ImGuiDockNodeFlags_NoDockingOverMe;
        }
    }
    ImGui::End();
    #endif
}

void Context::destroy() { ImGui::DestroyContext(); }

void Context::display_ui()
{
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
