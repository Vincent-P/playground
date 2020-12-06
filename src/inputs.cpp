#include "inputs.hpp"

#include "base/types.hpp"
#include "platform/window.hpp"
#include "ui.hpp"

#include <imgui/imgui.h>
#include <ranges>

namespace my_app
{

void Inputs::bind(Action action, const KeyBinding &binding) { bindings[action] = binding; }

bool Inputs::is_pressed(Action action)
{
    auto binding_it = bindings.find(action);
    if (binding_it != bindings.end())
    {
        const auto &binding = binding_it->second;
        return std::ranges::all_of(binding.keys, [&](VirtualKey key) { return is_pressed(key); })
               && std::ranges::all_of(binding.mouse_buttons, [&](MouseButton button) { return is_pressed(button); });
    }

    return false;
}

bool Inputs::is_pressed(VirtualKey key) { return keys_pressed[to_underlying(key)]; }

bool Inputs::is_pressed(MouseButton button) { return mouse_buttons_pressed[to_underlying(button)]; }

void Inputs::process(const std::vector<platform::event::Event> &events)
{
    scroll_this_frame = std::nullopt;
    auto last_mouse_position = mouse_position;

    for (const auto &event : events)
    {
        if (std::holds_alternative<platform::event::Key>(event))
        {
            const auto &key                      = std::get<platform::event::Key>(event);
            keys_pressed[to_underlying(key.key)] = key.state == ButtonState::Pressed;
        }
        else if (std::holds_alternative<platform::event::MouseClick>(event))
        {
            const auto &mouse_click                                  = std::get<platform::event::MouseClick>(event);
            mouse_buttons_pressed[to_underlying(mouse_click.button)] = mouse_click.state == ButtonState::Pressed;

            if (mouse_click.button == MouseButton::Left)
            {
                if (mouse_click.state == ButtonState::Pressed)
                {
                    if (!mouse_drag_start)
                    {
                        mouse_drag_start = mouse_position;
                    }
                }
                else
                {
                    mouse_drag_delta = std::nullopt;
                    mouse_drag_start = std::nullopt;
                }
            }
        }
        else if (std::holds_alternative<platform::event::Scroll>(event))
        {
            auto scroll = std::get<platform::event::Scroll>(event);

            if (scroll_this_frame)
            {
                scroll_this_frame->x += scroll.dx;
                scroll_this_frame->y += scroll.dy;
            }
            else
            {
                scroll_this_frame = {scroll.dx, scroll.dy};
            }
        }
        else if (std::holds_alternative<platform::event::MouseMove>(event))
        {
            auto move = std::get<platform::event::MouseMove>(event);
            last_mouse_position = {move.x, move.y};
        }
    }


    if (last_mouse_position.x != mouse_position.x || last_mouse_position.y != mouse_position.y)
    {
        mouse_delta = {last_mouse_position.x - mouse_position.x, last_mouse_position.y - mouse_position.y};
        mouse_position = {last_mouse_position.x, last_mouse_position.y};

        if (mouse_drag_start)
        {
            mouse_drag_delta = mouse_position - *mouse_drag_start;
        }
    }
    else
    {
        mouse_delta = std::nullopt;
    }
}

static void display_optional(const char *label, std::optional<int2> vector)
{
    ImGui::TextUnformatted(label);
    ImGui::SameLine();
    if (vector)
    {
        ImGui::Text("%dx%d", vector->x, vector->y);
    }
    else
    {
        ImGui::Text("none");
    }
}

void Inputs::display_ui(UI::Context &ui)
{
    if (ui.begin_window("Inputs"))
    {
        ImGui::Text("Keys");
        for (uint i = 0; i < to_underlying(VirtualKey::Count); i++)
        {
            auto key = static_cast<VirtualKey>(i);
            ImGui::Text("%s: %s", virtual_key_to_string(key), is_pressed(key) ? "Pressed" : "Released");
        }
        ImGui::Separator();
        ImGui::Text("Mouse buttons");
        for (uint i = 0; i < to_underlying(MouseButton::Count); i++)
        {
            auto button = static_cast<MouseButton>(i);
            ImGui::Text("%s: %s", mouse_button_to_string(button), is_pressed(button) ? "Pressed" : "Released");
        }
        ImGui::Separator();
        ImGui::Text("Mouse");
        ImGui::Text("position: %dx%d", mouse_position.x, mouse_position.y);
        display_optional("delta: ", mouse_delta);
        display_optional("mouse drag start: ", mouse_drag_start);
        display_optional("mouse drag delta: ", mouse_drag_delta);
        display_optional("scroll: ", scroll_this_frame);

        ui.end_window();
    }
}
} // namespace my_app
