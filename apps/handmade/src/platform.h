#pragma once
#include <cstdint>

enum struct PlatformType : uint32_t
{
	Win32,
	Count
};

struct PlatformWindow
{
	uint64_t display_handle;
	uint64_t window_handle;
};

struct GameState;

struct Platform
{
	PlatformType type;
	PlatformWindow *window;
	GameState *game_state;

	void (*debug_print)(const char *);

	using LoadLibraryFn = void *(*)(const char *);
	using GetLibraryProcFn = void *(*)(void *, const char *);
	using UnloadLibraryFn = void (*)(void *);
	LoadLibraryFn load_library;
	GetLibraryProcFn get_library_proc;
	UnloadLibraryFn unload_library;
};

using InitFunc = void (*)(Platform *);
using ReloadFn = void (*)(Platform *);
using UpdateFn = int (*)(Platform *);
using ShutdownFn = void (*)(Platform *);
