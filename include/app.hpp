#pragma once
#include "camera.hpp"
#include "types.hpp"
#include "file_watcher.hpp"
#include "renderer/renderer.hpp"
#include "timer.hpp"
#include "window.hpp"
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

    inline void display()
    {
        const auto& io = ImGui::GetIO();
        if (ImGui::BeginMainMenuBar())
        {
            ImGui::Dummy(ImVec2(io.DisplaySize.x / 2 - 100.0f, 0.0f));
            if (ImGui::BeginMenu(EVA_MENU " Menu"))
            {
                for (auto& [_, window] : windows) {
                    ImGui::MenuItem(window.name.c_str(), nullptr, &window.is_visible);
                }
                ImGui::EndMenu();
            }

            for (auto& [_, window] : windows) {
                if (window.is_visible) {
                    ImGui::MenuItem(window.name.c_str(), nullptr, &window.is_visible);
                }
            }

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

    NO_COPY_NO_MOVE(App)

    void run();

  private:
    void camera_update();
    void update();

    UI::Context ui;
    Window window;
    InputCamera camera;
    Renderer renderer;
    TimerData timer;

    FileWatcher watcher;
    Watch shaders_watch;
};

} // namespace my_app
