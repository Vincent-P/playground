#include "cross/platform.h"

#include <exo/logger.h>
#include <exo/macros/assert.h>

#include "utils_win32.h"

#include <windows.h>
#include <winuser.h>

static_assert(sizeof(DWORD) == sizeof(u32));

namespace cross
{
struct Platform
{
	u32   main_thread_id = 0;
	void *main_fiber     = nullptr;
};

usize platform_get_size() { return sizeof(Platform); }

// --- Window creation thread

Platform *platform_create(void *memory)
{
	auto *platform = reinterpret_cast<Platform *>(memory);

	platform->main_thread_id = GetCurrentThreadId();
	platform->main_fiber     = ConvertThreadToFiber(nullptr);

	return platform;
}

void platform_destroy(Platform *platform) {}

void *platform_win32_get_main_fiber(Platform *platform) { return platform->main_fiber; }
} // namespace cross
