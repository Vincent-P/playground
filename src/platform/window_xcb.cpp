#include "base/types.hpp"
#include "platform/window.hpp"

#include <cstdio>
#include <xcb/xproto.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <xkbcommon/xkbcommon-x11.h>
#include <xkbcommon/xkbcommon.h>

namespace window
{
std::array<uint, to_underlying(VirtualKey::Count) + 1> native_to_virtual{
#define X(EnumName, DisplayName, Win32, XKB) XKB,
#include "platform/window_keys.def"
#undef X
};

void Window::create(Window &window, usize width, usize height, std::string_view title)
{
    window.title  = std::string(title);
    window.width  = width;
    window.height = height;
    window.stop   = false;
    window.events.reserve(5); // should be good enough

    int screen_num        = 0;
    window.xcb.connection = xcb_connect(nullptr, &screen_num);

    const xcb_setup_t *setup = xcb_get_setup(window.xcb.connection);

    /// --- Get the active screen to create the window

    xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
    // skip iterator to go the the screen returnes by xcb_connect
    for (int i = 0; i < screen_num; i++)
    {
        xcb_screen_next(&iter);
    }
    xcb_screen_t *screen = iter.data;

    /// --- Create the window
    // specify which event we are listening to
    const static u32 values[] = {
        XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_POINTER_MOTION // mouse motion
            | XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE                         // mouse button
            | XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE,                              // keyboard inputs
    };

    window.xcb.window = xcb_generate_id(window.xcb.connection);
    xcb_create_window(window.xcb.connection, /* Pointer to the xcb_connection_t structure */
                      0,                     /* Depth of the screen */
                      window.xcb.window,     /* Id of the window */
                      screen->root,          /* Id of an existing window that should be the parent of the new window */
                      0,                     /* X position of the top-left corner of the window (in pixels) */
                      0,                     /* Y position of the top-left corner of the window (in pixels) */
                      width,                 /* Width of the window (in pixels) */
                      height,                /* Height of the window (in pixels) */
                      0,                     /* Width of the window's border (in pixels) */
                      XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      screen->root_visual,
                      XCB_CW_EVENT_MASK,
                      values);

    xcb_change_property(window.xcb.connection,
                        XCB_PROP_MODE_REPLACE,
                        window.xcb.window,
                        XCB_ATOM_WM_NAME,
                        XCB_ATOM_STRING,
                        8,
                        window.title.size(),
                        window.title.c_str());

    xcb_intern_atom_cookie_t cookie = xcb_intern_atom(window.xcb.connection, 1, 12, "WM_PROTOCOLS");
    xcb_intern_atom_reply_t *reply  = xcb_intern_atom_reply(window.xcb.connection, cookie, nullptr);

    xcb_intern_atom_cookie_t cookie2 = xcb_intern_atom(window.xcb.connection, 0, 16, "WM_DELETE_WINDOW");
    window.xcb.close_reply           = xcb_intern_atom_reply(window.xcb.connection, cookie2, nullptr);

    xcb_change_property(window.xcb.connection,
                        XCB_PROP_MODE_REPLACE,
                        window.xcb.window,
                        reply->atom,
                        4,
                        32,
                        1,
                        &window.xcb.close_reply->atom);

    // show window
    xcb_map_window(window.xcb.connection, window.xcb.window);

    /// --- Setup keyboard state using Xkbcommon

    window.xcb.kb_ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    assert(window.xcb.kb_ctx);
    window.xcb.device_id = xkb_x11_get_core_keyboard_device_id(window.xcb.connection);

    if (window.xcb.device_id != -1)
    {

        window.xcb.keymap = xkb_x11_keymap_new_from_device(window.xcb.kb_ctx,
                                                           window.xcb.connection,
                                                           window.xcb.device_id,
                                                           XKB_KEYMAP_COMPILE_NO_FLAGS);
    }
    else
    {
        struct xkb_rule_names names = {};
        window.xcb.keymap           = xkb_keymap_new_from_names(window.xcb.kb_ctx, &names, XKB_KEYMAP_COMPILE_NO_FLAGS);
    }

    assert(window.xcb.keymap);
    if (window.xcb.device_id != -1)
    {
        window.xcb.kb_state
            = xkb_x11_state_new_from_device(window.xcb.keymap, window.xcb.connection, window.xcb.device_id);
    }
    else
    {
        window.xcb.kb_state = xkb_state_new(window.xcb.keymap);
    }

    assert(window.xcb.kb_state);

    // flush commands
    xcb_flush(window.xcb.connection);
}

void Window::poll_events()
{
    xcb_generic_event_t *ev = nullptr;

    while ((ev = xcb_poll_for_event(xcb.connection)))
    {
        switch (ev->response_type & ~0x80)
        {
            case XCB_EXPOSE:
            {
                auto *expose = reinterpret_cast<xcb_expose_event_t *>(ev);
                (void)(expose);
                break;
            }

            case XCB_CLIENT_MESSAGE:
            {
                auto *client_message = reinterpret_cast<xcb_client_message_event_t *>(ev);
                if (client_message->data.data32[0] == xcb.close_reply->atom)
                {
                    stop = true;
                }
                break;
            }

            case XCB_BUTTON_PRESS:
            {
                auto *button_press = reinterpret_cast<xcb_button_press_event_t *>(ev);

                if (button_press->detail == 4)
                {
                    push_event<event::Scroll>({.dx = 0, .dy = -1});
                    break;
                }
                else if (button_press->detail == 5)
                {
                    push_event<event::Scroll>({.dx = 0, .dy = 1});
                    break;
                }

                MouseButton pressed = MouseButton::Count;
                switch (button_press->detail)
                {
                    case 1:
                        pressed = MouseButton::Left;
                        break;
                    case 2:
                        pressed = MouseButton::Middle;
                        break;
                    case 3:
                        pressed = MouseButton::Right;
                        break;
                    case 8:
                        pressed = MouseButton::SideBackward;
                        break;
                    case 9:
                        pressed = MouseButton::SideForward;
                        break;
                }

                if (pressed != MouseButton::Count)
                {
                    mouse_buttons_pressed[to_underlying(pressed)] = true;
                }

                break;
            }

            case XCB_BUTTON_RELEASE:
            {
                auto *button_release = reinterpret_cast<xcb_button_release_event_t *>(ev);

                MouseButton released = MouseButton::Count;
                switch (button_release->detail)
                {
                    case 1:
                        released = MouseButton::Left;
                        break;
                    case 2:
                        released = MouseButton::Middle;
                        break;
                    case 3:
                        released = MouseButton::Right;
                        break;
                    case 8:
                        released = MouseButton::SideBackward;
                        break;
                    case 9:
                        released = MouseButton::SideForward;
                        break;
                    default:
                        break;
                }

                if (released != MouseButton::Count)
                {
                    mouse_buttons_pressed[to_underlying(released)] = false;
                }
                break;
            }

            case XCB_MOTION_NOTIFY:
            {
                auto *motion = reinterpret_cast<xcb_motion_notify_event_t *>(ev);
                auto x       = motion->event_x;
                auto y       = motion->event_y;
                push_event<event::MouseMove>({x, y});
                mouse_position = float2(x, y);
                break;
            }

            case XCB_KEY_PRESS:
            case XCB_KEY_RELEASE:
            {
                // xcb_key_release_event_t is typedef'd to key_press
                auto *key_press = reinterpret_cast<xcb_key_press_event_t *>(ev);
                auto keycode    = key_press->detail;
                auto keysym     = xkb_state_key_get_one_sym(xcb.kb_state, keycode);

                auto key = VirtualKey::Count;
                for (uint i = 0; i < to_underlying(VirtualKey::Count); i++)
                {
                    uint xcb_virtual_key = native_to_virtual[i];
                    if (keysym == xcb_virtual_key)
                    {
                        key = static_cast<VirtualKey>(i);
                        break;
                    }
                }

                auto action = ev->response_type == XCB_KEY_PRESS ? event::Key::Action::Down : event::Key::Action::Up;

                push_event<event::Key>({.key = key, .action = action});
                keys_pressed[to_underlying(key)] = action == event::Key::Action::Down;

                break;
            }

            default:
            {
                /* Unknown event type, ignore it */
                break;
            }
        }

        free(ev);
    }
}

void Window::set_caret_pos(int2) {}

void Window::set_caret_size(int2) {}

void Window::remove_caret() {}

void Window::set_cursor(Cursor) {}

[[nodiscard]] float2 Window::get_dpi_scale() const { return float2(1.0f); }

void Window::destroy() { xcb_disconnect(xcb.connection); }
} // namespace window
