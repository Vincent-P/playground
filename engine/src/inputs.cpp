#include "inputs.h"

#include "ui.h"

#include <exo/option.h>
#include <exo/os/events.h>

#include <imgui.h>
#include <ranges>

void Inputs::bind(Action action, const KeyBinding &binding) { bindings[action] = binding; }

bool Inputs::is_pressed(Action action) const
{
    auto binding_it = bindings.find(action);
    if (binding_it != bindings.end())
    {
        const auto &binding = binding_it->second;
        return std::ranges::all_of(binding.keys, [&](exo::VirtualKey key) { return is_pressed(key); })
            && std::ranges::all_of(binding.mouse_buttons, [&](exo::MouseButton button) { return is_pressed(button); });
    }

    return false;
}

bool Inputs::is_pressed(exo::VirtualKey key) const { return keys_pressed[key]; }

bool Inputs::is_pressed(exo::MouseButton button) const { return mouse_buttons_pressed[button]; }

void Inputs::process(const Vec<exo::Event> &events)
{
    scroll_this_frame        = std::nullopt;
    auto last_mouse_position = mouse_position;

    for (const auto &event : events)
    {
        if (event.type == exo::Event::KeyType)
        {
            const auto &key       = event.key;
            keys_pressed[key.key] = key.state == exo::ButtonState::Pressed;
        }
        else if (event.type == exo::Event::MouseClickType)
        {
            const auto &mouse_click                   = event.mouse_click;
            mouse_buttons_pressed[mouse_click.button] = mouse_click.state == exo::ButtonState::Pressed;

            if (mouse_click.button == exo::MouseButton::Left)
            {
                if (mouse_click.state == exo::ButtonState::Pressed)
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
        else if (event.type == exo::Event::ScrollType)
        {
            const auto &scroll = event.scroll;

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
        else if (event.type == exo::Event::MouseMoveType)
        {
            const auto &move    = event.mouse_move;
            last_mouse_position = {move.x, move.y};
        }
    }

    if (last_mouse_position.x != mouse_position.x || last_mouse_position.y != mouse_position.y)
    {
        mouse_delta    = {last_mouse_position.x - mouse_position.x, last_mouse_position.y - mouse_position.y};
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

static void display_optional(const char *label, Option<int2> vector)
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

void Inputs::display_ui()
{
    ZoneScoped;
    if (auto w = UI::begin_window("Inputs"))
    {
        if (ImGui::CollapsingHeader("Keys"))
        {
            for (usize i = 0; i < static_cast<usize>(exo::VirtualKey::Count); i++)
            {
                auto key = static_cast<exo::VirtualKey>(i);
                ImGui::Text("%s: %s", to_string(key), is_pressed(key) ? "Pressed" : "Released");
            }
        }

        if (ImGui::CollapsingHeader("Mouse buttons"))
        {
            for (usize i = 0; i < static_cast<usize>(exo::MouseButton::Count); i++)
            {
                auto button = static_cast<exo::MouseButton>(i);
                ImGui::Text("%s: %s", to_string(button), is_pressed(button) ? "Pressed" : "Released");
            }
        }

        if (ImGui::CollapsingHeader("Mouse"))
        {
            ImGui::Text("position: %dx%d", mouse_position.x, mouse_position.y);
            display_optional("delta: ", mouse_delta);
            display_optional("mouse drag start: ", mouse_drag_start);
            display_optional("mouse drag delta: ", mouse_drag_delta);
            display_optional("scroll: ", scroll_this_frame);
        }

        if (ImGui::CollapsingHeader("Bindings"))
        {
            for (const auto &[action, binding] : bindings)
            {
                ImGui::Text("%s: ", to_string(action));
                for (const auto &key : binding.keys)
                {
                    ImGui::SameLine();
                    ImGui::Text("%s ", to_string(key));
                }
                for (const auto &mouse_button : binding.mouse_buttons)
                {
                    ImGui::SameLine();
                    ImGui::Text("%s ", to_string(mouse_button));
                }
            }
        }
    }
}
