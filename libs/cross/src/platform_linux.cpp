#include "cross/platform.h"
#include "exo/macros/assert.h"

namespace cross::platform
{
struct Platform
{
};

usize get_size() { return sizeof(Platform); }

// --- Window creation thread

void create(void *memory)
{
	ASSERT(!g_platform);
	g_platform = reinterpret_cast<Platform *>(memory);
}

void destroy() { ASSERT(g_platform); }

void *win32_get_main_fiber() { return nullptr; }
} // namespace cross::platform
