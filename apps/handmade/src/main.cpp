#include <cstdio>

#include <windows.h>
// subwindow below
#include <debugapi.h>

using InitFunc = void *(*)();
using ReloadFn = void (*)(void *);
using UpdateFn = int (*)(void *);
using ShutdownFn = void (*)(void *);

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

int APIENTRY WinMain(HINSTANCE instance, HINSTANCE previous_instance, PSTR cmdline, int cmdshow)
{
	(void)(instance);
	(void)(previous_instance);
	(void)(cmdline);
	(void)(cmdshow);

	const char *game_path = GAME_DLL_PATH;
	const char *game_tmp_path = GAME_DLL_PATH ".tmp";

	DynamicModule game = {};
	char buffer[1024] = {};

	load_dynamic_module(&game, game_path, game_tmp_path);
	void *game_state = game.init_fn();

	int value = 2;
	for (;;) {
		if (value == 0)
			break;

		FILETIME game_module_date = win32_get_last_write_time(game_path);
		bool is_game_up_to_date = game.latest_update.dwHighDateTime == game_module_date.dwHighDateTime &&
		                          game.latest_update.dwLowDateTime == game_module_date.dwLowDateTime;
		if (!is_game_up_to_date) {
			unload_dynamic_module(&game);
			load_dynamic_module(&game, game_path, game_tmp_path);
			game.reload_fn(game_state);
		}

		// Write value on the debugger
		auto written = snprintf(buffer, sizeof(buffer), "Value: %d\n", value);
		buffer[written + 1] = 0;
		OutputDebugStringA(buffer);

		value = game.update_fn(game_state);
	}
	game.shutdown_fn(game_state);

	return MessageBoxW(NULL, L"End.", L"The", 0);
}
