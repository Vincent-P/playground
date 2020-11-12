#pragma once
#include "base/types.hpp"

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#if defined(_WIN64)
typedef struct HWND__ *HWND;
typedef struct HGLRC__ *HGLRC;
#else
#    include <xcb/xcb.h>
struct xkb_context;
struct xkb_keymap;
struct xkb_state;
#endif

namespace window
{
enum struct MouseButton : uint
{
    Left,
    Right,
    Middle,
    SideForward,
    SideBackward,
    Count
};

inline constexpr std::array<const char *, to_underlying(MouseButton::Count) + 1> mouse_button_to_string{
    "Left mouse button",
    "Right mouse button",
    "Middle mouse button (wheel)",
    "Side mouse button forward",
    "Side mouse button backward",
    "COUNT",
};

enum struct VirtualKey : uint
{
#define X(EnumName, DisplayName, Win32, Xlib) EnumName,
#include "window_keys.h"
#undef X
};

inline constexpr std::array<const char *, to_underlying(VirtualKey::Count) + 1> key_to_string{
#define X(EnumName, DisplayName, Win32, Xlib) DisplayName,
#include "platform/window_keys.h"
#undef X
};

inline constexpr const char *virtual_key_to_string(VirtualKey key) { return key_to_string[to_underlying(key)]; }

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

namespace event
{

struct Key
{
    enum class Action
    {
        Down,
        Up
    };

    Key(VirtualKey _key, Action _action)
        : key(_key)
        , action(_action)
    {
    }
    VirtualKey key;
    Action action;
};

struct Char
{
    Char(const std::string &str)
        : char_sequence(str)
    {
    }
    std::string char_sequence;
};

struct IMEComposition
{
    IMEComposition(const std::string &str)
        : composition(str)
    {
    }
    std::string composition;
};

struct IMECompositionResult
{
    IMECompositionResult(const std::string &str)
        : result(str)
    {
    }
    std::string result;
};

struct Scroll
{
    Scroll(int _dx, int _dy)
        : dx(_dx)
        , dy(_dy)
    {
    }
    int dx;
    int dy;
};

struct MouseMove
{
    MouseMove(int _x, int _y)
        : x(_x)
        , y(_y)
    {
    }
    int x;
    int y;
};

struct Focus
{
    bool focused;
};

struct Resize
{
    Resize(uint _width, uint _height)
        : width(_width)
        , height(_height)
    {
    }
    uint width;
    uint height;
};

using Event = std::variant<Key, Char, IMEComposition, IMECompositionResult, Scroll, MouseMove, Focus, Resize>;

} // namespace event

using Event = event::Event;

struct Caret
{
    int2 position;
    int2 size;
};

struct Window;

#if defined(_WIN64)
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
    static void create(Window &window, usize width, usize height, const std::string_view title);
    ~Window() = default;

    void destroy();

    void poll_events();

    // caret operations
    void set_caret_pos(int2 pos);
    void set_caret_size(int2 size);
    void remove_caret();

    // cursor operations
    void set_cursor(Cursor cursor);

    [[nodiscard]] inline bool should_close() const { return stop; }

    [[nodiscard]] float2 get_dpi_scale() const;

    [[nodiscard]] inline bool is_key_pressed(VirtualKey key) const { return keys_pressed[to_underlying(key)]; }
    [[nodiscard]] inline bool is_mouse_button_pressed(MouseButton button) const
    {
        return mouse_buttons_pressed[to_underlying(button)];
    }
    [[nodiscard]] inline float2 get_mouse_position() const { return mouse_position; }

    // push events to the events vector
    template <typename EventType, class... Args> void emit_event(Args &&...args)
    {
        events.emplace_back(std::in_place_type<EventType>, std::forward<Args>(args)...);
    }

    std::string title;
    uint width;
    uint height;
    float2 mouse_position;

    bool stop{false};
    std::optional<Caret> caret{};

    bool has_focus{false};
    bool minimized{false};
    bool maximized{false};
    Cursor current_cursor;

    std::vector<Event> events;

    std::array<bool, to_underlying(VirtualKey::Count) + 1> keys_pressed           = {};
    std::array<bool, to_underlying(MouseButton::Count) + 1> mouse_buttons_pressed = {};

#if defined(_WIN64)
    Window_Win32 win32;
#else
    Window_Xcb xcb;
#endif
};

} // namespace window
