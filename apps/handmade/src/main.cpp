#include <cstdint>
#include <cstdio>

#include <windows.h>
// subwindow below
#include <debugapi.h>

struct PlatformWindow
{
	uint64_t display_handle;
	uint64_t window_handle;
};

using InitFunc = void *(*)(PlatformWindow *);
using ReloadFn = void (*)(void *);
using UpdateFn = int (*)(void *);
using ShutdownFn = void (*)(void *);

// -- Win32

struct DynamicModule
{
	HMODULE dll_module;
	FILETIME latest_update;
	InitFunc init_fn;
	ReloadFn reload_fn;
	UpdateFn update_fn;
	ShutdownFn shutdown_fn;
};

FILETIME win32_get_last_write_time(const char *path)
{

	WIN32_FILE_ATTRIBUTE_DATA data = {};
	GetFileAttributesExA(path, GetFileExInfoStandard, &data);
	return data.ftLastWriteTime;
}

void load_dynamic_module(DynamicModule *module, const char *path, const char *tmp_path)
{
	CopyFileExA(path, tmp_path, nullptr, nullptr, nullptr, false);

	module->dll_module = LoadLibraryA(tmp_path);
	module->init_fn = (InitFunc)GetProcAddress(module->dll_module, "init");
	module->reload_fn = (ReloadFn)GetProcAddress(module->dll_module, "reload");
	module->update_fn = (UpdateFn)GetProcAddress(module->dll_module, "update");
	module->shutdown_fn = (ShutdownFn)GetProcAddress(module->dll_module, "shutdown");

	module->latest_update = win32_get_last_write_time(path);
}

void unload_dynamic_module(DynamicModule *module)
{
	FreeLibrary(module->dll_module);
	module->dll_module = NULL;
	module->init_fn = nullptr;
	module->reload_fn = nullptr;
	module->update_fn = nullptr;
	module->shutdown_fn = nullptr;
}

struct Win32Window : PlatformWindow
{
	bool stop;
};

static LRESULT CALLBACK window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	// Set or get the user dat aassociated to the window
	Win32Window *window = nullptr;
	if (uMsg == WM_CREATE) {
		auto *p_create = reinterpret_cast<CREATESTRUCT *>(lParam);
		window = static_cast<Win32Window *>(p_create->lpCreateParams);
		SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
	} else {
		LONG_PTR const ptr = GetWindowLongPtrW(hwnd, GWLP_USERDATA);
		window = reinterpret_cast<Win32Window *>(ptr);
	}

	switch (uMsg) {
	case WM_CREATE: {
		return 0;
	}

	case WM_CLOSE: {
		window->stop = true;
		break;
	}

	case WM_DESTROY: {
		PostQuitMessage(0);
		return 0;
	}

	case WM_SETFOCUS: {
		// window.has_focus = true;
		return 0;
	}
	}

	// default message handling
	return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

int APIENTRY WinMain(HINSTANCE instance, HINSTANCE previous_instance, PSTR cmdline, int cmdshow)
{
	(void)(previous_instance); // Always NULL...
	(void)(cmdline);           // Unused when UNICODE, use GetCommandLineW

	// Create window
	Win32Window win32_window = {};

	// Register the window class
	WNDCLASS wc = {};
	wc.lpfnWndProc = window_proc;
	wc.hInstance = instance;
	wc.lpszClassName = L"Playground class";
	wc.style = CS_OWNDC;
	RegisterClassW(&wc);

	// Create the window instance
	auto hwnd = CreateWindowExW(WS_EX_TRANSPARENT, // Optional window styles.
		wc.lpszClassName,                          // Window class
		L"Playground",                             // Window text
		WS_BORDER | WS_OVERLAPPEDWINDOW,           // Window style
		// Position and size
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		// Width height
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		nullptr,      // Parent window
		nullptr,      // Menu
		instance,     // Instance handle
		&win32_window // Additional application data
	);

	ShowWindow(hwnd, cmdshow);

	// Prepare window data for the game
	win32_window.window_handle = (uint64_t)(hwnd);

	// Load game DLL
	const char *game_path = GAME_DLL_PATH;
	const char *game_tmp_path = GAME_DLL_PATH ".tmp";
	DynamicModule game = {};
	load_dynamic_module(&game, game_path, game_tmp_path);
	void *game_state = game.init_fn(&win32_window);

	char buffer[1024] = {};
	int value = 2;
	MSG msg = {};
	for (; !win32_window.stop;) {
		if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}

		// Reload DLL
		FILETIME game_module_date = win32_get_last_write_time(game_path);
		bool is_game_up_to_date = game.latest_update.dwHighDateTime == game_module_date.dwHighDateTime &&
		                          game.latest_update.dwLowDateTime == game_module_date.dwLowDateTime;
		if (!is_game_up_to_date) {
			unload_dynamic_module(&game);
			load_dynamic_module(&game, game_path, game_tmp_path);
			game.reload_fn(game_state);
		}

		// Update
		auto written = snprintf(buffer, sizeof(buffer), "Value: %d\n", value);
		buffer[written + 1] = 0;
		OutputDebugStringA(buffer);
		value = game.update_fn(game_state);
	}
	game.shutdown_fn(game_state);
	unload_dynamic_module(&game);

	return MessageBoxW(NULL, L"End.", L"The", MB_DEFAULT_DESKTOP_ONLY);
}
