#include "window.hpp"

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

inline bool is_high_surrogate(wchar_t c)
{
    return 0xD800 <= c && c <= 0xDBFF;
}
inline bool is_low_surrogate(wchar_t c)
{
    return 0xDC00 <= c && c <= 0xDFFF;
}

namespace window
{
std::array<uint, to_underlying(VirtualKey::Count) + 1> native_to_virtual{
#define X(EnumName, DisplayName, Win32, Xlib) Win32,
#include "window_keys.h"
#undef X
};

std::array<const char *, to_underlying(VirtualKey::Count) + 1> key_to_string{
#define X(EnumName, DisplayName, Win32, Xlib) DisplayName,
#include "window_keys.h"
#undef X
};

inline Window *get_window_from_handle(HWND hwnd)
{
    LONG_PTR ptr = GetWindowLongPtr(hwnd, GWLP_USERDATA);
    auto *window = reinterpret_cast<Window *>(ptr);
    return window;
}

static LRESULT CALLBACK window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

void Window::create(Window &window, usize width, usize height, std::string_view title)
{
    window.width  = width;
    window.height = height;
    window.title  = utf8_to_utf16(title);
    window.stop   = false;
    window.events.reserve(5); // should be good enough

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
                          window.title.c_str(),            // Window text
                          WS_BORDER | WS_OVERLAPPEDWINDOW, // Window style
                          // Position and size
                          CW_USEDEFAULT,
                          CW_USEDEFAULT,
                          // Width height
                          static_cast<int>(window.width),
                          static_cast<int>(window.height),
                          nullptr,     // Parent window
                          nullptr,     // Menu
                          instance,    // Instance handle
                          &window      // Additional application data
    );

    if (!hwnd)
    {
        throw std::runtime_error{"Could not create window instance."};
    }

    ShowWindow(hwnd, SW_SHOW);
}

uint2 Window::get_dpi_scale() const
{
    // int dpi = GetDpiForWindow(win32.window);
    int dpi = 1;
    return uint2(dpi, dpi);
}

void Window::poll_events()
{
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
            window.emit_event<event::Resize>(window.width, window.height);
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

            auto action = event::Key::Action::Down;
            if (uMsg == WM_KEYUP)
            {
                action = event::Key::Action::Up;
            }

            window.emit_event<event::Key>(key, action);
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
                    window.emit_event<event::Char>("\n");
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
                        window.emit_event<event::Char>(utf16_to_utf8(buffer.data()));
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
                    window.emit_event<event::IMEComposition>(utf16_to_utf8(result));
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
                    window.emit_event<event::IMECompositionResult>(utf16_to_utf8(result));
                }
            }

            return 0;
        }

        case WM_IME_ENDCOMPOSITION:
        {
            window.emit_event<event::IMEComposition>("");
        }

        /// --- Mouse inputs

        case WM_MOUSEWHEEL:
        {
            // fwKeys = GET_KEYSTATE_WPARAM(wParam);
            int delta = GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;
            window.emit_event<event::Scroll>(0, -delta);
            return 0;
        }

        case WM_MOUSEMOVE:
        {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            window.emit_event<event::MouseMove>(x, y);
            return 0;
        }
    }

    // default message handling
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

} // namespace window
