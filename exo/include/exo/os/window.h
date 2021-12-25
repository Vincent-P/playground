#pragma once
#include "exo/option.h"
#include "exo/maths/vectors.h"
#include "exo/collections/vector.h"
#include "exo/collections/enum_array.h"
#include "exo/os/buttons.h"
#include "exo/os/events.h"

#include <string>
#include <string_view>

namespace exo { struct ScopeStack; }

namespace exo
{
using PaintFunction = void (*)(void *user_data);

enum struct Cursor
{
    None,
    Arrow,
    TextInput,
    ResizeAll,
    ResizeEW,
    ResizeNS,
    ResizeNESW,
    ResizeNWSE,
    Hand,
    NotAllowed
};

struct Caret
{
    int2 position{0};
    int2 size{0};
};

struct Window
{
    static Window *create(ScopeStack &scope, int2 size, const std::string_view title);
    ~Window() = default;

    void set_title(std::string_view new_title);
    void poll_events();

    // caret operations
    void set_caret_pos(int2 pos);
    void set_caret_size(int2 size);
    void remove_caret();

    // cursor operations
    void set_cursor(Cursor cursor);

    [[nodiscard]] float2 get_dpi_scale() const;

    [[nodiscard]] inline bool should_close() const { return stop; }
    [[nodiscard]] inline bool is_key_pressed(VirtualKey key) const { return keys_pressed[key]; }
    [[nodiscard]] inline bool is_mouse_button_pressed(MouseButton button) const { return mouse_buttons_pressed[button]; }
    [[nodiscard]] inline float2 get_mouse_position() const { return mouse_position; }
    void *get_win32_hwnd() const;

    std::string title;
    int2 size;
    bool stop{false};
    Cursor current_cursor;

    float2 mouse_position = {0.0f};
    Option<Caret> caret{};

    bool has_focus{false};
    bool minimized{false};
    bool maximized{false};

    Vec<Event> events;

    EnumArray<bool, VirtualKey> keys_pressed           = {};
    EnumArray<bool, MouseButton> mouse_buttons_pressed = {};
    void *native_data;
    void *user_data;
    PaintFunction paint_callback;
};

} // namespace exo
