#include "cross/window.h"
#include "exo/collections/enum_array.h"
#include <cstdio>
#include <xcb/xproto.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <xkbcommon/xkbcommon-x11.h>
#include <xkbcommon/xkbcommon.h>

exo::EnumArray<uint, cross::VirtualKey> native_to_virtual{
#define X(EnumName, DisplayName, Win32, XKB) XKB,
#include "cross/keyboard_keys.def"
#undef X
};

namespace cross
{
struct Window::Impl
{
	Impl() = default;
	~Impl();

	xcb_connection_t *connection;
	u32 window;
	xcb_intern_atom_reply_t *close_reply;
	xkb_context *kb_ctx;
	i32 device_id;
	xkb_keymap *keymap;
	xkb_state *kb_state;
};

std::unique_ptr<Window> Window::create(int2 size, const exo::StringView title)
{
	auto window = std::make_unique<Window>();
	window->title = exo::String(title);
	window->size = size;
	window->stop = false;
	window->events.reserve(5); // should be good enough

	auto &window_impl = window->impl.get();
	int screen_num = 0;
	window_impl.connection = xcb_connect(nullptr, &screen_num);

	const xcb_setup_t *setup = xcb_get_setup(window_impl.connection);

	/// --- Get the active screen to create the window

	xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
	// skip iterator to go the the screen returnes by xcb_connect
	for (int i = 0; i < screen_num; i++) {
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

	window_impl.window = xcb_generate_id(window_impl.connection);
	xcb_create_window(window_impl.connection, /* Pointer to the xcb_connection_t structure */
		0,                                    /* Depth of the screen */
		window_impl.window,                   /* Id of the window */
		screen->root,                         /* Id of an existing window that should be the parent of the new window */
		0,                                    /* X position of the top-left corner of the window (in pixels) */
		0,                                    /* Y position of the top-left corner of the window (in pixels) */
		u16(size.x),                          /* Width of the window (in pixels) */
		u16(size.y),                          /* Height of the window (in pixels) */
		0,                                    /* Width of the window's border (in pixels) */
		XCB_WINDOW_CLASS_INPUT_OUTPUT,
		screen->root_visual,
		XCB_CW_EVENT_MASK,
		values);

	xcb_change_property(window_impl.connection,
		XCB_PROP_MODE_REPLACE,
		window_impl.window,
		XCB_ATOM_WM_NAME,
		XCB_ATOM_STRING,
		8,
		window->title.len(),
		window->title.c_str());

	xcb_intern_atom_cookie_t cookie = xcb_intern_atom(window_impl.connection, 1, 12, "WM_PROTOCOLS");
	xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(window_impl.connection, cookie, nullptr);

	xcb_intern_atom_cookie_t cookie2 = xcb_intern_atom(window_impl.connection, 0, 16, "WM_DELETE_WINDOW");
	window_impl.close_reply = xcb_intern_atom_reply(window_impl.connection, cookie2, nullptr);

	xcb_change_property(window_impl.connection,
		XCB_PROP_MODE_REPLACE,
		window_impl.window,
		reply->atom,
		4,
		32,
		1,
		&window_impl.close_reply->atom);

	// show window
	xcb_map_window(window_impl.connection, window_impl.window);

	/// --- Setup keyboard state using Xkbcommon

	window_impl.kb_ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	ASSERT(window_impl.kb_ctx);
	window_impl.device_id = xkb_x11_get_core_keyboard_device_id(window_impl.connection);

	if (window_impl.device_id != -1) {

		window_impl.keymap = xkb_x11_keymap_new_from_device(window_impl.kb_ctx,
			window_impl.connection,
			window_impl.device_id,
			XKB_KEYMAP_COMPILE_NO_FLAGS);
	} else {
		struct xkb_rule_names names = {};
		window_impl.keymap = xkb_keymap_new_from_names(window_impl.kb_ctx, &names, XKB_KEYMAP_COMPILE_NO_FLAGS);
	}

	ASSERT(window_impl.keymap);
	if (window_impl.device_id != -1) {
		window_impl.kb_state =
			xkb_x11_state_new_from_device(window_impl.keymap, window_impl.connection, window_impl.device_id);
	} else {
		window_impl.kb_state = xkb_state_new(window_impl.keymap);
	}

	ASSERT(window_impl.kb_state);

	// flush commands
	xcb_flush(window_impl.connection);

	return window;
}

Window::Impl::~Impl() { xcb_disconnect(this->connection); }

void Window::set_title(exo::StringView new_title)
{
	this->title = new_title;
	auto &xcb = this->impl.get();

	xcb_change_property(xcb.connection,
		XCB_PROP_MODE_REPLACE,
		xcb.window,
		XCB_ATOM_WM_NAME,
		XCB_ATOM_STRING,
		8,
		this->title.len(),
		this->title.c_str());
	xcb_flush(xcb.connection);
}

void Window::poll_events()
{
	auto &xcb = this->impl.get();
	xcb_generic_event_t *ev = nullptr;

	while ((ev = xcb_poll_for_event(xcb.connection))) {
		switch (ev->response_type & ~0x80) {
		case XCB_EXPOSE: {
			auto *expose = reinterpret_cast<xcb_expose_event_t *>(ev);
			(void)(expose);
			break;
		}

		case XCB_CLIENT_MESSAGE: {
			auto *client_message = reinterpret_cast<xcb_client_message_event_t *>(ev);
			if (client_message->data.data32[0] == xcb.close_reply->atom) {
				stop = true;
			}
			break;
		}

		case XCB_CONFIGURE_NOTIFY: {
			auto *configure_message = reinterpret_cast<xcb_configure_notify_event_t *>(ev);
			// push_event<event::Resize>({.width = configure_message->width, .height = configure_message->height});
			break;
		}

		case XCB_BUTTON_PRESS: {
			auto *button_press = reinterpret_cast<xcb_button_press_event_t *>(ev);

			if (button_press->detail == 4) {
				this->events.push(Event{.type = Event::ScrollType, .scroll = {.dx = 0, .dy = -1}});
				break;
			} else if (button_press->detail == 5) {
				this->events.push(Event{.type = Event::ScrollType, .scroll = {.dx = 0, .dy = 1}});
				break;
			}

			MouseButton pressed = MouseButton::Count;
			switch (button_press->detail) {
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

			if (pressed != MouseButton::Count) {
				events.push(Event{.type = Event::MouseClickType,
					.mouse_click = {.button = pressed, .state = ButtonState::Pressed}});
				mouse_buttons_pressed[pressed] = true;
			}

			break;
		}

		case XCB_BUTTON_RELEASE: {
			auto *button_release = reinterpret_cast<xcb_button_release_event_t *>(ev);

			MouseButton released = MouseButton::Count;
			switch (button_release->detail) {
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

			if (released != MouseButton::Count) {
				events.push(Event{.type = Event::MouseClickType,
					.mouse_click = {.button = released, .state = ButtonState::Released}});
				mouse_buttons_pressed[released] = false;
			}
			break;
		}

		case XCB_MOTION_NOTIFY: {
			auto *motion = reinterpret_cast<xcb_motion_notify_event_t *>(ev);
			auto x = motion->event_x;
			auto y = motion->event_y;
			events.push(Event{.type = Event::MouseMoveType, .mouse_move = {x, y}});
			mouse_position = {x, y};
			break;
		}

		case XCB_KEY_PRESS:
		case XCB_KEY_RELEASE: {
			// xcb_key_release_event_t is typedef'd to key_press
			auto *key_press = reinterpret_cast<xcb_key_press_event_t *>(ev);
			auto keycode = key_press->detail;
			auto keysym = xkb_state_key_get_one_sym(xcb.kb_state, keycode);

			auto key = VirtualKey::Count;
			for (uint i = 0; i < static_cast<usize>(VirtualKey::Count); i++) {
				auto virtual_key = static_cast<VirtualKey>(i);
				uint xcb_virtual_key = native_to_virtual[virtual_key];
				if (keysym == xcb_virtual_key) {
					key = static_cast<VirtualKey>(i);
					break;
				}
			}

			if (key != VirtualKey::Count) {
				auto state = ev->response_type == XCB_KEY_PRESS ? ButtonState::Pressed : ButtonState::Released;
				events.push(Event{Event::KeyType, {events::Key{.key = key, .state = state}}});
				keys_pressed[key] = state == ButtonState::Pressed;
			}
			break;
		}

		default: {
			/* Unknown event type, ignore it */
			break;
		}
		}

		free(ev);
	}
}

void Window::set_cursor(Cursor) {}

[[nodiscard]] float2 Window::get_dpi_scale() const { return float2(1.0f); }

u64 Window::get_display_handle() const { return (u64)impl.get().connection; }
u64 Window::get_window_handle() const { return u64(impl.get().window); }
} // namespace cross
