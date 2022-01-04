#include "exo/os/window.h"

#include "exo/logger.h"
#include "exo/memory/scope_stack.h"

#include "utils_win32.h"

#include <string>
#include <windows.h>
#include <windowsx.h>
#include <imm.h>

namespace exo
{

namespace
{
struct WindowWin32
{
    HWND wnd;
};

static WindowWin32 &impl(Window &window)
{
    return *reinterpret_cast<WindowWin32 *>(window.native_data);
}
static const WindowWin32 &impl(const Window &window)
{
    return *reinterpret_cast<const WindowWin32 *>(window.native_data);
}

static bool is_high_surrogate(wchar_t c)
{
    return 0xD800 <= c && c <= 0xDBFF;
}
static bool is_low_surrogate(wchar_t c)
{
    return 0xDC00 <= c && c <= 0xDFFF;
}

EnumArray<i32, VirtualKey> native_to_virtual{
#define X(EnumName, DisplayName, Win32, Xlib) Win32,
#include "exo/os/window_keys.def"
#undef X
};

// Some keys on windows don't get updated with a message and need to be polled manually
static void update_key_state(Window &window, VirtualKey key)
{
    auto old        = window.keys_pressed[key];
    auto native_key = native_to_virtual[key];
    auto pressed    = (GetKeyState(native_key) & 0x8000) != 0;

    window.keys_pressed[key] = pressed;

    if (old != pressed)
    {
        auto state = ButtonState::Pressed;
        if (old)
        {
            state = ButtonState::Released;
        }
        window.events.push_back({Event::KeyType, events::Key{.key = key, .state = state}});
    }
}
} // namespace

static LRESULT CALLBACK window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

Window *Window::create(ScopeStack &scope, int2 size, const std::string_view title)
{
    auto *window  = scope.allocate<Window>();
    window->title = std::string(title);
    window->size  = size;
    window->stop  = false;
    window->events.reserve(5); // should be good enough
    window->native_data = scope.allocate<WindowWin32>();

    auto utf16_title = utf8_to_utf16(title);

    HINSTANCE instance = GetModuleHandle(nullptr);

    // Register the window class.
    WNDCLASS wc      = {};
    wc.lpfnWndProc   = window_proc;
    wc.hInstance     = instance;
    wc.lpszClassName = L"SupEd Window Class";
    wc.style         = CS_OWNDC;
    RegisterClass(&wc);

    // Create the window instance
    HWND &hwnd = impl(*window).wnd;
    hwnd       = CreateWindowEx(WS_EX_TRANSPARENT,               // Optional window styles.
                          wc.lpszClassName,                // Window class
                          utf16_title.c_str(),             // Window text
                          WS_BORDER | WS_OVERLAPPEDWINDOW, // Window style
                          // Position and size
                          CW_USEDEFAULT,
                          CW_USEDEFAULT,
                          // Width height
                          window->size.x,
                          window->size.y,
                          nullptr,  // Parent window
                          nullptr,  // Menu
                          instance, // Instance handle
                          window    // Additional application data
    );

    ASSERT(hwnd);
    ShowWindow(hwnd, SW_SHOW);
    return window;
}


void *Window::get_win32_hwnd() const
{
    return impl(*this).wnd;
}

float2 Window::get_dpi_scale() const
{
    uint  dpi   = GetDpiForWindow(impl(*this).wnd);
    float scale = static_cast<float>(dpi) / 96.0f;
    if (scale <= 0.0f)
    {
        scale = 1.0f;
    }
    return float2(scale, scale);
}

void Window::set_title(std::string_view new_title)
{
    this->title      = std::string{new_title};
    auto utf16_title = utf8_to_utf16(title);
    auto res         = SetWindowTextW(impl(*this).wnd, utf16_title.c_str());
    ASSERT(res != 0);
}

void Window::poll_events()
{
    // need to take care of shit, control and alt manually...
    update_key_state(*this, VirtualKey::LShift);
    update_key_state(*this, VirtualKey::RShift);
    update_key_state(*this, VirtualKey::LControl);
    update_key_state(*this, VirtualKey::RControl);
    update_key_state(*this, VirtualKey::LAlt);
    update_key_state(*this, VirtualKey::RAlt);

    MSG msg;
    if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

void Window::set_caret_pos(int2 pos)
{
    if (!caret)
    {
        caret.emplace<Caret>({});
    }
    caret->position = pos;

    DestroyCaret();

    auto wnd = impl(*this).wnd;

    CreateCaret(wnd, (HBITMAP) nullptr, caret->size.x, caret->size.y);
    SetCaretPos(caret->position.x, caret->position.y);
    ShowCaret(wnd);
}

void Window::set_caret_size(int2 size)
{
    if (!caret)
    {
        caret.emplace<Caret>({});
    }
    caret->size = size;
}

void Window::remove_caret()
{
    DestroyCaret();
    caret = std::nullopt;
}

void Window::set_cursor(Cursor cursor)
{
    LPTSTR win32_cursor = nullptr;
    switch (cursor)
    {
    case Cursor::None:
        break;
    case Cursor::Arrow:
        win32_cursor = IDC_ARROW;
        break;
    case Cursor::TextInput:
        win32_cursor = IDC_IBEAM;
        break;
    case Cursor::ResizeAll:
        win32_cursor = IDC_SIZEALL;
        break;
    case Cursor::ResizeEW:
        win32_cursor = IDC_SIZEWE;
        break;
    case Cursor::ResizeNS:
        win32_cursor = IDC_SIZENS;
        break;
    case Cursor::ResizeNESW:
        win32_cursor = IDC_SIZENESW;
        break;
    case Cursor::ResizeNWSE:
        win32_cursor = IDC_SIZENWSE;
        break;
    case Cursor::Hand:
        win32_cursor = IDC_HAND;
        break;
    case Cursor::NotAllowed:
        win32_cursor = IDC_NO;
        break;
    }
    ::SetCursor(cursor == Cursor::None ? nullptr : ::LoadCursor(nullptr, win32_cursor));
    current_cursor = cursor;
}

// Window callback function (handles window messages)
static LRESULT CALLBACK window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    // Get the window from the user pointer
    Window *tmp;
    if (uMsg == WM_CREATE)
    {
        auto *p_create = reinterpret_cast<CREATESTRUCT *>(lParam);
        tmp            = reinterpret_cast<Window *>(p_create->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)tmp);
    }
    else
    {
        LONG_PTR ptr = GetWindowLongPtr(hwnd, GWLP_USERDATA);
        tmp          = reinterpret_cast<Window *>(ptr);
    }

    Window &window = *tmp;

    switch (uMsg)
    {
    case WM_CREATE:
    {
        // no need to create gl context
        return 0;
    }

    case WM_CLOSE:
    {
        window.stop = true;
        break;
    }

    case WM_DESTROY:
    {
        PostQuitMessage(0);
        return 0;
    }

    case WM_SETFOCUS:
    {
        window.has_focus = true;

        if (window.caret)
        {
            CreateCaret(hwnd, (HBITMAP) nullptr, window.caret->size.x, window.caret->size.y);
            SetCaretPos(window.caret->position.x, window.caret->position.y);
            ShowCaret(hwnd);
        }

        return 0;
    }

    case WM_KILLFOCUS:
    {
        window.has_focus = false;
        if (window.caret)
        {
            DestroyCaret();
        }

        return 0;
    }

    case WM_SIZE:
    {
        window.minimized = wParam == SIZE_MINIMIZED;
        window.maximized = wParam == SIZE_MAXIMIZED;

        window.size = {LOWORD(lParam), HIWORD(lParam)};
        // window.push_event<event::Resize>({.width = window.width, .height = window.height});
        logger::info("WM_SIZE: {}x{}\n", window.size.x, window.size.y);
        return 0;
    }

    case WM_PAINT:
    {
        if (window.paint_callback) {
            window.paint_callback(window.user_data);
        }
        return 0;
    }

    // --- Keyboard Inputs

    case WM_KEYUP:
    case WM_KEYDOWN:
    {
        auto key = VirtualKey::Count;
        for (usize i = 0; i < static_cast<usize>(VirtualKey::Count); i++)
        {
            auto virtual_key       = static_cast<VirtualKey>(i);
            auto win32_virtual_key = native_to_virtual[virtual_key];
            if (static_cast<i32>(wParam) == win32_virtual_key)
            {
                key = virtual_key;
                break;
            }
        }

        if (key != VirtualKey::Count)
        {
            auto state = ButtonState::Pressed;
            if (uMsg == WM_KEYUP)
            {
                state = ButtonState::Released;
            }

            window.events.push_back({Event::KeyType, events::Key{.key = key, .state = state}});
            window.keys_pressed[key] = state == ButtonState::Pressed;
        }
        return 0;
    }

    case WM_SYSKEYUP:
    case WM_SYSKEYDOWN:
    {
        return 0;
    }

    case WM_CHAR:
    {
        switch (wParam)
        {
        case 0x08:
        {
            // Process a backspace.
            window.events.push_back({.type = Event::CharacterType, .character = {.sequence = "\b"}});
            break;
        }
        case 0x0A:
        {
            // Process a linefeed.
            window.events.push_back({.type = Event::CharacterType, .character = {.sequence = "\n"}});
            break;
        }
        case 0x1B:
        {
            // Process an escape.
            window.events.push_back({.type = Event::CharacterType, .character = {.sequence = "\e"}});
            break;
        }
        case 0x09:
        {
            // Process a tab.
            window.events.push_back({.type = Event::CharacterType, .character = {.sequence = "\t"}});
            break;
        }
        case 0x0D:
        {
            // Process a carriage return.
            window.events.push_back({.type = Event::CharacterType, .character = {.sequence = "\n"}});
            break;
        }
        default:
        {
            // Process displayable characters.
            static wchar_t buffer[3] = {0, 0, 0};

            wchar_t codepoint = static_cast<wchar_t>(wParam);
            bool    clear     = false;

            if (is_high_surrogate(codepoint))
            {
                buffer[0] = codepoint;
            }
            else if (is_low_surrogate(codepoint))
            {
                buffer[1] = codepoint;
                clear     = true;
            }
            else
            {
                buffer[0] = codepoint;
                clear     = true;
            }

            if (clear)
            {
                auto utf8_buffer = utf16_to_utf8(buffer);
                ASSERT(utf8_buffer.size() < sizeof(events::Character));
                window.events.push_back({.type = Event::CharacterType, .character = {.sequence = {utf8_buffer[0]}}});
                buffer[0] = 0;
                buffer[1] = 0;
                buffer[2] = 0;
            }
            break;
        }
        }

        return 0;
    }

    // Handle input methods: emoji picker or CJK keyboards for example
    case WM_IME_COMPOSITION:
    {
        HIMC himc = ImmGetContext(hwnd);
        if (lParam & GCS_COMPSTR) // Retrieve or update the reading string of the current composition.
        {
            i32 res = ImmGetCompositionString(himc, GCS_COMPSTR, nullptr, 0);
            ASSERT(res > 0);
            u32 size = static_cast<u32>(res);
            if (size)
            {
                std::wstring result;
                result.resize(size);
                ImmGetCompositionStringW(himc, GCS_COMPSTR, reinterpret_cast<void *>(result.data()), size);
                auto utf8_result = utf16_to_utf8(result);
                window.events.push_back({.type = Event::IMECompositionType, .ime_composition = {nullptr}});
            }
        }
        else if (lParam & GCS_RESULTSTR) // Retrieve or update the string of the composition result.
        {
            i32 res = ImmGetCompositionString(himc, GCS_RESULTSTR, nullptr, 0);
            ASSERT(res > 0);
            u32 size = static_cast<u32>(res);
            if (size)
            {
                std::wstring result;
                result.resize(size);
                ImmGetCompositionStringW(himc, GCS_RESULTSTR, reinterpret_cast<void *>(result.data()), size);
                auto utf8_result = utf16_to_utf8(result);
                window.events.push_back({.type = Event::IMECompositionResultType, .ime_composition_result = {nullptr}});
            }
        }

        return 0;
    }

    case WM_IME_ENDCOMPOSITION:
    {
        window.events.push_back({.type = Event::IMECompositionType, .ime_composition = {.composition = nullptr}});
    }

        /// --- Mouse inputs

    case WM_MOUSEWHEEL:
    {
        // fwKeys = GET_KEYSTATE_WPARAM(wParam);
        int delta = GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;
        window.events.push_back({.type = Event::ScrollType, .scroll = {.dx = 0, .dy = -delta}});
        return 0;
    }

    case WM_MOUSEMOVE:
    {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        window.events.push_back({.type = Event::MouseMoveType, .mouse_move = {x, y}});
        window.mouse_position = {x, y};
        return 0;
    }

    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONDBLCLK:
    case WM_XBUTTONDOWN:
    case WM_XBUTTONDBLCLK:
    {
        MouseButton button = MouseButton::Count;
        if (uMsg == WM_LBUTTONDOWN || uMsg == WM_LBUTTONDBLCLK)
        {
            button = MouseButton::Left;
        }
        if (uMsg == WM_RBUTTONDOWN || uMsg == WM_RBUTTONDBLCLK)
        {
            button = MouseButton::Right;
        }
        if (uMsg == WM_MBUTTONDOWN || uMsg == WM_MBUTTONDBLCLK)
        {
            button = MouseButton::Middle;
        }
        if (uMsg == WM_XBUTTONDOWN || uMsg == WM_XBUTTONDBLCLK)
        {
            button = (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) ? MouseButton::SideForward : MouseButton::SideBackward;
        }
        window.events.push_back(
            {.type = Event::MouseClickType, .mouse_click = {.button = button, .state = ButtonState::Pressed}});
        window.mouse_buttons_pressed[button] = true;
        return 0;
    }

    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
    case WM_XBUTTONUP:
    {
        MouseButton button = MouseButton::Count;
        if (uMsg == WM_LBUTTONUP)
        {
            button = MouseButton::Left;
        }
        if (uMsg == WM_RBUTTONUP)
        {
            button = MouseButton::Right;
        }
        if (uMsg == WM_MBUTTONUP)
        {
            button = MouseButton::Middle;
        }
        if (uMsg == WM_XBUTTONUP)
        {
            button = (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) ? MouseButton::SideForward : MouseButton::SideBackward;
        }
        window.events.push_back(
            {.type = Event::MouseClickType, .mouse_click = {.button = button, .state = ButtonState::Released}});
        window.mouse_buttons_pressed[button] = true;
        return 0;
    }
    }

    // default message handling
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

} // namespace exo
