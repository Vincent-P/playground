#include "platform/window.hpp"

#include <cassert>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <windows.h>
#include <windowsx.h>
// clang-format off
#include <imm.h>
// clang-format on

/// --- Text utils functions

static std::wstring utf8_to_utf16(const std::string_view &str)
{
    if (str.empty())
    {
        return {};
    }

    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), nullptr, 0);
    std::wstring result(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), result.data(), result.size());
    return result;
}

static std::string utf16_to_utf8(const std::wstring_view &wstr)
{
    if (wstr.empty())
    {
        return {};
    }

    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
    std::string result(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), result.data(), result.size(), nullptr, nullptr);
    return result;
}

inline bool is_high_surrogate(wchar_t c) { return 0xD800 <= c && c <= 0xDBFF; }
inline bool is_low_surrogate(wchar_t c) { return 0xDC00 <= c && c <= 0xDFFF; }

namespace my_app
{
std::array<uint, to_underlying(VirtualKey::Count) + 1> native_to_virtual{
#define X(EnumName, DisplayName, Win32, Xlib) Win32,
#include "platform/window_keys.def"
#undef X
};

namespace platform
{

inline Window *get_window_from_handle(HWND hwnd)
{
    LONG_PTR ptr = GetWindowLongPtr(hwnd, GWLP_USERDATA);
    auto *window = reinterpret_cast<Window *>(ptr);
    return window;
}

static LRESULT CALLBACK window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

void Window::create(Window &window, usize width, usize height, std::string_view title)
{
    window.title  = std::string(title);
    window.width  = width;
    window.height = height;
    window.stop   = false;
    window.events.reserve(5); // should be good enough

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
    HWND &hwnd = window.win32.window;
    hwnd       = CreateWindowEx(WS_EX_TRANSPARENT,               // Optional window styles.
                          wc.lpszClassName,                // Window class
                          utf16_title.c_str(),             // Window text
                          WS_BORDER | WS_OVERLAPPEDWINDOW, // Window style
                          // Position and size
                          CW_USEDEFAULT,
                          CW_USEDEFAULT,
                          // Width height
                          static_cast<int>(window.width),
                          static_cast<int>(window.height),
                          nullptr,  // Parent window
                          nullptr,  // Menu
                          instance, // Instance handle
                          &window   // Additional application data
    );

    if (!hwnd)
    {
        throw std::runtime_error{"Could not create window instance."};
    }

    ShowWindow(hwnd, SW_SHOW);
}

void Window::destroy() {}

float2 Window::get_dpi_scale() const
{
    int dpi     = GetDpiForWindow(win32.window);
    float scale = dpi / 96.0f;
    return float2(scale, scale);
}

static void update_key(Window &window, VirtualKey key)
{
    auto old         = window.keys_pressed[to_underlying(key)];
    auto native_key  = native_to_virtual[to_underlying(key)];
    auto pressed     = (GetKeyState(native_key) & 0x8000) != 0;

    window.keys_pressed[to_underlying(key)] = pressed;

    if (old != pressed)
    {
        auto state = ButtonState::Pressed;
        if (old)
        {
            state = ButtonState::Released;
        }
        window.push_event<event::Key>({.key = key, .state = state});
    }
}

void Window::poll_events()
{
    // need to take care of shit, control and alt manually...
    update_key(*this, VirtualKey::LShift);
    update_key(*this, VirtualKey::RShift);
    update_key(*this, VirtualKey::LControl);
    update_key(*this, VirtualKey::RControl);
    update_key(*this, VirtualKey::LAlt);
    update_key(*this, VirtualKey::RAlt);

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

    CreateCaret(win32.window, (HBITMAP) nullptr, caret->size.x, caret->size.y);
    SetCaretPos(caret->position.x, caret->position.y);
    ShowCaret(win32.window);
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
        tmp = get_window_from_handle(hwnd);
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

            window.width  = LOWORD(lParam);
            window.height = HIWORD(lParam);
            window.push_event<event::Resize>({.width = window.width, .height = window.height});
            return 0;
        }

            /// --- Keyboard Inputs

        case WM_KEYUP:
        case WM_KEYDOWN:
        {
            auto key = VirtualKey::Count;
            for (uint i = 0; i < to_underlying(VirtualKey::Count); i++)
            {
                uint win32_virtual_key = native_to_virtual[i];
                if (wParam == win32_virtual_key)
                {
                    key = static_cast<VirtualKey>(i);
                    break;
                }
            }

            auto state = ButtonState::Pressed;
            if (uMsg == WM_KEYUP)
            {
                state = ButtonState::Released;
            }

            window.push_event<event::Key>({.key = key, .state = state});
            window.keys_pressed[to_underlying(key)] = state == ButtonState::Pressed;
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
                    break;
                }
                case 0x0A:
                {
                    // Process a linefeed.
                    break;
                }
                case 0x1B:
                {
                    // Process an escape.

                    break;
                }
                case 0x09:
                {
                    // Process a tab.

                    break;
                }
                case 0x0D:
                {
                    // Process a carriage return.
                    window.push_event<event::Char>({"\n"});
                    break;
                }
                default:
                {
                    // Process displayable characters.
                    static std::array<wchar_t, 3> buffer{};

                    wchar_t codepoint = wParam;
                    bool clear        = false;

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
                        window.push_event<event::Char>({utf16_to_utf8(buffer.data())});
                        buffer.fill(0);
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
                auto size = ImmGetCompositionString(himc, GCS_COMPSTR, nullptr, 0);
                if (size)
                {
                    std::wstring result;
                    result.resize(size);
                    ImmGetCompositionStringW(himc, GCS_COMPSTR, reinterpret_cast<void *>(result.data()), size);
                    window.push_event<event::IMEComposition>({utf16_to_utf8(result)});
                }
            }
            else if (lParam & GCS_RESULTSTR) // Retrieve or update the string of the composition result.
            {
                auto size = ImmGetCompositionString(himc, GCS_RESULTSTR, nullptr, 0);
                if (size)
                {
                    std::wstring result;
                    result.resize(size);
                    ImmGetCompositionStringW(himc, GCS_RESULTSTR, reinterpret_cast<void *>(result.data()), size);
                    window.push_event<event::IMECompositionResult>({utf16_to_utf8(result)});
                }
            }

            return 0;
        }

        case WM_IME_ENDCOMPOSITION:
        {
            window.push_event<event::IMEComposition>({""});
        }

            /// --- Mouse inputs

        case WM_MOUSEWHEEL:
        {
            // fwKeys = GET_KEYSTATE_WPARAM(wParam);
            int delta = GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;
            window.push_event<event::Scroll>({.dx = 0, .dy = -delta});
            return 0;
        }

        case WM_MOUSEMOVE:
        {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            window.push_event<event::MouseMove>({x, y});
            window.mouse_position = float2(x, y);
            return 0;
        }

        // clang-format off
        case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK:
        case WM_XBUTTONDOWN: case WM_XBUTTONDBLCLK:
            // clang-format on
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
                window.push_event<event::MouseClick>({.button = button, .state = ButtonState::Pressed});
                window.mouse_buttons_pressed[to_underlying(button)] = true;
                return 0;
            }

        // clang-format off
        case WM_LBUTTONUP: case WM_RBUTTONUP:
        case WM_MBUTTONUP: case WM_XBUTTONUP:
            // clang-format on
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
                window.push_event<event::MouseClick>({.button = button, .state = ButtonState::Released});
                window.mouse_buttons_pressed[to_underlying(button)] = true;
                return 0;
            }
    }

    // default message handling
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

} // namespace platform
} // namespace my_app
