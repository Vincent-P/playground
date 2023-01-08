#include "common.h"
#include "platform.h"
#include <cstdio>
#include <windows.h>

// -- Win32

struct Win32Window : PlatformWindow
{
	bool stop;
};

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

static LRESULT CALLBACK window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	// Set or get the user dat aassociated to the window
	Platform *platform = nullptr;
	if (uMsg == WM_CREATE) {
		auto *p_create = reinterpret_cast<CREATESTRUCT *>(lParam);
		platform = (Platform *)p_create->lpCreateParams;
		SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)platform);
	} else {
		LONG_PTR const ptr = GetWindowLongPtrW(hwnd, GWLP_USERDATA);
		platform = (Platform *)ptr;
	}

	Win32Window *window = platform ? (Win32Window *)platform->window : nullptr;

	LRESULT result = 0;
	switch (uMsg) {
	case WM_DESTROY: {
		// Will post WM_CLOSE
		PostQuitMessage(0);
		break;
	}

	case WM_CLOSE: {
		window->stop = true;
		result = DefWindowProcW(hwnd, uMsg, wParam, lParam);
		break;
	}

	// default message handling
	default: {
		result = DefWindowProcW(hwnd, uMsg, wParam, lParam);
	}
	}

	return result;
}

HWND win32_create_window(HINSTANCE instance, void *user_data)
{
	// Register the window class
	WNDCLASS wc = {};
	wc.lpfnWndProc = window_proc;
	wc.hInstance = instance;
	wc.lpszClassName = L"Playground class";
	wc.style = CS_OWNDC;
	RegisterClassW(&wc);

	// Create the window instance
	return CreateWindowExW(WS_EX_TRANSPARENT, // Optional window styles.
		wc.lpszClassName,                     // Window class
		L"Playground",                        // Window text
		WS_BORDER | WS_OVERLAPPEDWINDOW,      // Window style
		// Position and size
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		// Width height
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		nullptr,  // Parent window
		nullptr,  // Menu
		instance, // Instance handle
		user_data // Additional application data
	);
}

int APIENTRY WinMain(HINSTANCE instance, HINSTANCE previous_instance, PSTR cmdline, int cmdshow)
{
	(void)(previous_instance); // Always NULL...
	(void)(cmdline);           // Unused when UNICODE, use GetCommandLineW

	// Fill platform API
	Win32Window win32_window = {};
	Platform platform = {};
	platform.window = &win32_window;
	platform.debug_print = OutputDebugStringA;
	platform.load_library = (Platform::LoadLibraryFn)LoadLibraryA;
	platform.get_library_proc = (Platform::GetLibraryProcFn)GetProcAddress;
	platform.unload_library = (Platform::UnloadLibraryFn)FreeLibrary;

	// Create window
	auto hwnd = win32_create_window(instance, &platform);
	win32_window.window_handle = (uint64_t)(hwnd);
	ShowWindow(hwnd, cmdshow);

	// Load game DLL
	const char *game_path = GAME_DLL_PATH;
	const char *game_tmp_path = GAME_DLL_PATH ".tmp";
	DynamicModule game = {};
	load_dynamic_module(&game, game_path, game_tmp_path);
	game.init_fn(&platform);

	MSG msg = {};
	for (; !win32_window.stop;) {
		// Win32 message loop
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
			game.reload_fn(&platform);

			auto b = make_tester(33);
			b.bark();
		}

		game.update_fn(&platform);
	}
	game.shutdown_fn(&platform);
	unload_dynamic_module(&game);

	return MessageBoxW(NULL, L"End.", L"The", MB_DEFAULT_DESKTOP_ONLY);
}
