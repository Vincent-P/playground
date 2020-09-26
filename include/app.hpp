#pragma once
#include "camera.hpp"
#include "base/types.hpp"
#include "file_watcher.hpp"
#include "renderer/renderer.hpp"
#include "timer.hpp"
#include "platform/window.hpp"
#include <imgui/imgui.h>
#include "eva-icons.hpp"
#include <unordered_map>
#include <string>

namespace my_app
{

namespace UI
{
struct Window
{
    std::string name;
    bool is_visible;
};

struct Context
{
    inline bool begin_window(std::string_view name, bool is_visible = false, ImGuiWindowFlags flags = 0)
    {
        if (windows.count(name)) {
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

    inline void end_window()
    {
        ImGui::End();
    }

    inline void display_ui()
    {
        const auto& io = ImGui::GetIO();
        if (ImGui::BeginMainMenuBar())
        {
            auto display_width = io.DisplaySize.x * io.DisplayFramebufferScale.x;
            ImGui::Dummy(ImVec2(display_width / 2 - 100.0f * io.DisplayFramebufferScale.x, 0.0f));

            ImGui::TextUnformatted("|");
            if (ImGui::BeginMenu(EVA_MENU " Menu"))
            {
                for (auto& [_, window] : windows) {
                    ImGui::MenuItem(window.name.c_str(), nullptr, &window.is_visible);
                }
                ImGui::EndMenu();
            }

            for (auto& [_, window] : windows) {
                if (window.is_visible) {
                    ImGui::TextUnformatted("|");
                    ImGui::MenuItem(window.name.c_str(), nullptr, &window.is_visible);
                }
            }
            ImGui::TextUnformatted("|");

            ImGui::EndMainMenuBar();

        }
    }

    std::unordered_map<std::string_view, Window> windows;
};
}

class App
{
  public:
    App();
    ~App();

    void run();

  private:
    void camera_update();
    void update();
    void display_ui();

    UI::Context ui;
    window::Window window;
    InputCamera camera;
    Renderer renderer;
    TimerData timer;

    FileWatcher watcher;
    Watch shaders_watch;

    bool is_minimized;
};

} // namespace my_app
