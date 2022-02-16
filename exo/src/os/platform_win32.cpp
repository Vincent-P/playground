#include "exo/os/platform.h"
#include <windows.h>

#include "utils_win32.h"

static_assert(sizeof(DWORD) == sizeof(u32));
#define CREATE_DANGEROUS_WINDOW (WM_USER + 0x1337)
#define DESTROY_DANGEROUS_WINDOW (WM_USER + 0x1338)

struct WindowCreationParams
{
	exo::int2 size;
	std::string_view title;
};

namespace exo
{
struct Platform
{
	u32 main_thread_id;
	u32 window_creation_thread_id;
	HWND window_creation_window;
};

usize platform_get_size() { return sizeof(Platform); }

// --- Window creation thread

struct the_baby
{
	DWORD     dwExStyle;
	LPCWSTR   lpClassName;
	LPCWSTR   lpWindowName;
	DWORD     dwStyle;
	int       X;
	int       Y;
	int       nWidth;
	int       nHeight;
	HWND      hWndParent;
	HMENU     hMenu;
	HINSTANCE hInstance;
	LPVOID    lpParam;
};

static LRESULT CALLBACK window_creation_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
	LRESULT result = 0;

    // Get the platform from the user pointer
    Platform *platform;
    if (message == WM_CREATE)
    {
        auto *p_create = reinterpret_cast<CREATESTRUCT *>(lparam);
        platform            = reinterpret_cast<Platform *>(p_create->lpCreateParams);
        SetWindowLongPtr(window, GWLP_USERDATA, (LONG_PTR)platform);
    }
    else
    {
        LONG_PTR ptr = GetWindowLongPtr(window, GWLP_USERDATA);
        platform          = reinterpret_cast<Platform *>(ptr);
    }

	switch (message) {
	case CREATE_DANGEROUS_WINDOW: {
            the_baby *Baby = (the_baby *)wparam;
            result = (LRESULT)CreateWindowExW(Baby->dwExStyle,
                                                  Baby->lpClassName,
                                                  Baby->lpWindowName,
                                                  Baby->dwStyle,
                                                  Baby->X,
                                                  Baby->Y,
                                                  Baby->nWidth,
                                                  Baby->nHeight,
                                                  Baby->hWndParent,
                                                  Baby->hMenu,
                                                  Baby->hInstance,
                                                  Baby->lpParam);
			break;
	}

	case DESTROY_DANGEROUS_WINDOW: {
		DestroyWindow((HWND)wparam);
		break;
	}

	default:
	{
		result = DefWindowProcW(window, message, wparam, lparam);
		break;
	}
	}

	return result;
}

static DWORD WINAPI window_creation_thread(void *param)
{
	auto *platform = reinterpret_cast<Platform*>(param);

	WNDCLASSEXW WindowClass   = {};
	WindowClass.cbSize        = sizeof(WindowClass);
	WindowClass.lpfnWndProc   = &window_creation_proc;
	WindowClass.hInstance     = GetModuleHandleW(nullptr);
	WindowClass.hIcon         = LoadIcon(nullptr, IDI_APPLICATION);
	WindowClass.hCursor       = LoadCursor(nullptr, IDC_ARROW);
	WindowClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	WindowClass.lpszClassName = L"WindowCreationClass";
	RegisterClassExW(&WindowClass);

	platform->window_creation_window = CreateWindowExW(0, WindowClass.lpszClassName, L"WindowCreationWindow", 0, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, WindowClass.hInstance, platform);

	// This background "window creation" thread will forward all interesting events back to the main thread.
	// All other messages will go through the default window proc, as well as custom messages to create or destroy windows.
    for(;;)
    {
        MSG message = {};
        GetMessageW(&message, 0, 0, 0);
        TranslateMessage(&message);

        if((message.message == WM_CHAR) ||
           (message.message == WM_KEYDOWN) ||
           (message.message == WM_QUIT) ||
           (message.message == WM_SIZE))
        {
            PostThreadMessageW(platform->main_thread_id, message.message, message.wParam, message.lParam);
        }
        else
        {
            DispatchMessageW(&message);
        }
    }

	return 0;
}

// --- Window forwarding


static LRESULT CALLBACK window_forward_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
	LRESULT result = 0;
    // Get the platform from the user pointer
    Platform *platform;
    if (message == WM_CREATE)
    {
        auto *p_create = reinterpret_cast<CREATESTRUCT *>(lparam);
        platform            = reinterpret_cast<Platform *>(p_create->lpCreateParams);
        SetWindowLongPtr(window, GWLP_USERDATA, (LONG_PTR)platform);
    }
    else
    {
        LONG_PTR ptr = GetWindowLongPtr(window, GWLP_USERDATA);
        platform          = reinterpret_cast<Platform *>(ptr);
    }

    switch (message)
    {
        // NOTE(casey): Mildly annoying, if you want to specify a window, you have
        // to snuggle the params yourself, because Windows doesn't let you forward
        // a god damn window message even though the program IS CALLED WINDOWS. It's
        // in the name! Let me pass it!
        case WM_CLOSE:
        {
            PostThreadMessageW(platform->main_thread_id, message, (WPARAM)window, lparam);
        } break;

        // NOTE(casey): Anything you want the application to handle, forward to the main thread
        // here.
        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_DESTROY:
        case WM_CHAR:
	case WM_SIZE:
        {
            PostThreadMessageW(platform->main_thread_id, message, wparam, lparam);
        } break;

        default:
        {
            result = DefWindowProcW(window, message, wparam, lparam);
        } break;
    }

    return result;
}

// ---

Platform *platform_create(void *memory)
{
	auto *platform = reinterpret_cast<Platform *>(memory);

	platform->main_thread_id = GetCurrentThreadId();

	// All window creation/deletion are made in a background thread.
	// All messages from these windows are then sent back to the main thread using this hidden window event loop
	CreateThread(0, 0, window_creation_thread, memory, 0, (LPDWORD)&platform->window_creation_thread_id);

	volatile int i = 0;
	while (true) {
		if (platform->window_creation_window != NULL)
		{
			break;
		}
		i += 1;
	}

	return platform;
}

void platform_destroy(Platform *platform)
{
}

u64 platform_create_window(Platform *platform, int2 size, std::string_view title)
{
	auto utf16_title = utf8_to_utf16(title);

	WNDCLASSEXW WindowClass   = {};
	WindowClass.cbSize        = sizeof(WindowClass);
	WindowClass.lpfnWndProc   = window_forward_proc;
	WindowClass.hInstance     = GetModuleHandleW(NULL);
	WindowClass.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
	WindowClass.hCursor       = LoadCursor(NULL, IDC_ARROW);
	WindowClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	WindowClass.lpszClassName = L"ExoWindowClass";
	WindowClass.style         = CS_OWNDC;
	RegisterClassExW(&WindowClass);

	the_baby Baby  = {};
	Baby.dwExStyle = WS_EX_TRANSPARENT;
	Baby.lpClassName  = WindowClass.lpszClassName;
	Baby.lpWindowName = utf16_title.c_str();
	Baby.dwStyle      = WS_BORDER | WS_OVERLAPPEDWINDOW;
	Baby.X            = CW_USEDEFAULT;
	Baby.Y            = CW_USEDEFAULT;
	Baby.nWidth       = size.x;
	Baby.nHeight      = size.y;
	Baby.hInstance    = WindowClass.hInstance;
	Baby.lpParam      = platform;

	HWND window = (HWND)SendMessageW(platform->window_creation_window, CREATE_DANGEROUS_WINDOW, (WPARAM)&Baby, 0);
	return (u64)window;
}
} // namespace exo
