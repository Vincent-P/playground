#include "ui.h"

#include <engine/inputs.h>

bool ui_is_hovering(const UiState &ui_state, const Rect &rect)
{
    return rect.position.x <= ui_state.inputs->mouse_position.x && ui_state.inputs->mouse_position.x <= rect.position.x + rect.size.x
        && rect.position.y <= ui_state.inputs->mouse_position.y && ui_state.inputs->mouse_position.y <= rect.position.y + rect.size.y;
}

u64 ui_make_id(UiState &state)
{
    return ++state.gen;
}

void ui_new_frame(UiState &ui_state)
{
    ui_state.gen = 0;
    ui_state.focused = 0;
}

void ui_end_frame(UiState &ui_state)
{
    if (!ui_state.inputs->mouse_buttons_pressed[exo::MouseButton::Left])
    {
        ui_state.active = 0;
    }
    else
    {
        // active = invalid means no widget are focused
        if (ui_state.active == 0)
        {
            ui_state.active = u64_invalid;
        }
    }
}

bool ui_button(UiState &ui_state, const UiTheme &ui_theme, const UiButton &button)
{
    bool result = false;
    u64 id = ui_make_id(ui_state);

    // behavior
    if (ui_is_hovering(ui_state, button.rect))
    {
        ui_state.focused = id;
        if (ui_state.active == 0 && ui_state.inputs->mouse_buttons_pressed[exo::MouseButton::Left])
        {
            ui_state.active = id;
        }
    }

    if (!ui_state.inputs->mouse_buttons_pressed[exo::MouseButton::Left] && ui_state.focused == id && ui_state.active == id)
    {
        result = true;
    }

    // rendering
    if (ui_state.focused == id)
    {
        if (ui_state.active == id)
        {
            painter_draw_color_rect(*ui_state.painter, button.rect, ui_theme.button_pressed_bg_color);
        }
        else
        {
            painter_draw_color_rect(*ui_state.painter, button.rect, ui_theme.button_hover_bg_color);
        }
    }
    else
    {
        painter_draw_color_rect(*ui_state.painter, button.rect, ui_theme.button_bg_color);
    }

    auto label_rect = button.rect;
    label_rect.size = exo::float2(measure_label(ui_theme.main_font, button.label));
    label_rect.position.x += (button.rect.size.x - label_rect.size.x) / 2.0f;
    label_rect.position.y += (button.rect.size.y - label_rect.size.y) / 2.0f;

    // painter_draw_color_rect(*ui_state.painter, label_rect, 0xFF0000FF);
    painter_draw_label(*ui_state.painter, label_rect, ui_theme.main_font, button.label);

    return result;
}

void ui_splitter_x(UiState &ui_state, const UiTheme &ui_theme, const Rect &view_rect, float &value, Rect &left, Rect &right)
{
    u64 id = ui_make_id(ui_state);

    Rect splitter_input = Rect{.position = {view_rect.position.x + view_rect.size.x * value - 5.0f, view_rect.position.y}, .size = {10.0f, view_rect.size.y}};

    // behavior
    if (ui_is_hovering(ui_state, splitter_input))
    {
        ui_state.focused = id;
        if (ui_state.active == 0 && ui_state.inputs->mouse_buttons_pressed[exo::MouseButton::Left])
        {
            ui_state.active = id;
        }
    }

    if (ui_state.active == id)
    {
        value = float(ui_state.inputs->mouse_position.x - view_rect.position.x) / float(view_rect.size.x);
    }

    left.position = view_rect.position;
    left.size = {view_rect.size.x * value, view_rect.size.y};

    right.position = {view_rect.position.x + left.size.x, view_rect.position.y};
    right.size = {view_rect.size.x * (1.0f - value), view_rect.size.y};

    // painter_draw_color_rect(*ui_state.painter, splitter_input, 0x440000FF);
    if (ui_state.focused == id)
    {
        painter_draw_color_rect(*ui_state.painter, {.position = {right.position.x - 2, view_rect.position.y}, .size = {4, view_rect.size.y}}, 0xFF888888);
    }
    else
    {
        painter_draw_color_rect(*ui_state.painter, {.position = {right.position.x - 1, view_rect.position.y}, .size = {2, view_rect.size.y}}, 0xFF666666);
    }
}

void ui_splitter_y(UiState &ui_state, const UiTheme &ui_theme, const Rect &view_rect, float &value, Rect &top, Rect &bottom)
{
    u64 id = ui_make_id(ui_state);

    Rect splitter_input = Rect{.position = {view_rect.position.x, view_rect.position.y + view_rect.size.y * value - 5.0f }, .size = {view_rect.size.x, 10.0f}};

    // behavior
    if (ui_is_hovering(ui_state, splitter_input))
    {
        ui_state.focused = id;
        if (ui_state.active == 0 && ui_state.inputs->mouse_buttons_pressed[exo::MouseButton::Left])
        {
            ui_state.active = id;
        }
    }

    if (ui_state.active == id)
    {
        value = float(ui_state.inputs->mouse_position.y - view_rect.position.y) / float(view_rect.size.y);
    }

    top.position = view_rect.position;
    top.size = {view_rect.size.x, view_rect.size.y * value};

    bottom.position = {view_rect.position.x, view_rect.position.y + top.size.y};
    bottom.size = {view_rect.size.x, view_rect.size.y * (1.0f - value)};

    // painter_draw_color_rect(*ui_state.painter, splitter_input, 0x440000FF);
    if (ui_state.focused == id)
    {
        painter_draw_color_rect(*ui_state.painter, {.position = {view_rect.position.x, bottom.position.y - 2}, .size = {view_rect.size.x, 4}}, 0xFF888888);
    }
    else
    {
        painter_draw_color_rect(*ui_state.painter, {.position = {view_rect.position.x, bottom.position.y - 1}, .size = {view_rect.size.x, 2}}, 0xFF666666);
    }

    // painter_draw_color_rect(*ui_state.painter, splitter_input, 0x440000FF);
}

void ui_label(UiState &ui_state, const UiTheme &ui_theme, const UiLabel &label)
{
    // painter_draw_color_rect(*ui_state.painter, label.rect, 0xFF0000FF);
    painter_draw_label(*ui_state.painter, label.rect, ui_theme.main_font, label.text);
}
