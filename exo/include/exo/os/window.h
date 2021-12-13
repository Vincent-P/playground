#pragma once
#include "exo/option.h"
#include "exo/maths/vectors.h"
#include "exo/collections/vector.h"
#include "exo/collections/enum_array.h"
#include "exo/os/prelude.h"
#include "exo/os/buttons.h"
#include "exo/os/events.h"

#include <string>
#include <string_view>

#if defined(CROSS_WINDOWS)
typedef struct HWND__ *HWND;
typedef struct HGLRC__ *HGLRC;
#else
#    include <xcb/xcb.h>
struct xkb_context;
struct xkb_keymap;
struct xkb_state;
#endif

namespace exo
{
struct ScopeStack;
}

namespace exo
{
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

struct Window;

#if defined(CROSS_WINDOWS)
struct Window_Win32
{
    HWND window;
};
#else
struct Window_Xcb
{
    xcb_connection_t *connection;
    xcb_window_t window;

    int device_id;
    xkb_context *kb_ctx;
    xkb_state *kb_state;
    xkb_keymap *keymap;
    xcb_intern_atom_reply_t *close_reply;
};
#endif

struct Window
{
    static Window *create(ScopeStack &scope, u32 width, u32 height, const std::string_view title);
    ~Window();

    void set_title(std::string_view new_title);
    void poll_events();

    // caret operations
    void set_caret_pos(int2 pos);
    void set_caret_size(int2 size);
    void remove_caret();

    // cursor operations
    void set_cursor(Cursor cursor);

    [[nodiscard]] inline bool should_close() const { return stop; }

    [[nodiscard]] float2 get_dpi_scale() const;

    [[nodiscard]] inline bool is_key_pressed(VirtualKey key) const { return keys_pressed[key]; }
    [[nodiscard]] inline bool is_mouse_button_pressed(MouseButton button) const
    {
        return mouse_buttons_pressed[button];
    }
    [[nodiscard]] inline float2 get_mouse_position() const { return mouse_position; }

    std::string title;
    uint width;
    uint height;
    float2 mouse_position = {0.0f};

    bool stop{false};
    Option<Caret> caret{};

    bool has_focus{false};
    bool minimized{false};
    bool maximized{false};
    Cursor current_cursor;

    Vec<Event> events;

    EnumArray<bool, VirtualKey> keys_pressed           = {};
    EnumArray<bool, MouseButton> mouse_buttons_pressed = {};

#if defined(CROSS_WINDOWS)
    Window_Win32 win32;
#else
    Window_Xcb xcb;
#endif
};

} // namespace exo
