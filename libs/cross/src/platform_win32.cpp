#include "cross/platform.h"

#include "exo/macros/assert.h"

#include <shellscalingapi.h> // for dpi awareness
#include <windows.h>
#include <winuser.h>

static_assert(sizeof(DWORD) == sizeof(u32));

namespace cross::platform
{
struct Platform
{
	u32   main_thread_id = 0;
	void *main_fiber     = nullptr;
};

usize get_size() { return sizeof(Platform); }

// --- Window creation thread

void create(void *memory)
{
	ASSERT(!g_platform);
	g_platform = reinterpret_cast<Platform *>(memory);

	g_platform->main_thread_id = GetCurrentThreadId();
	g_platform->main_fiber     = ConvertThreadToFiber(nullptr);

	HRESULT res = 0;
	res         = SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
	ASSERT(SUCCEEDED(res));
}

void destroy() { ASSERT(g_platform); }

void *win32_get_main_fiber() { return g_platform->main_fiber; }
} // namespace cross::platform
