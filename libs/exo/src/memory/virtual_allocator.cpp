#include "exo/memory/virtual_allocator.h"

#include "exo/logger.h"
#include "exo/macros/assert.h"

#if defined(_WIN32)
#include <windows.h>
#endif

namespace exo::virtual_allocator
{
u32 get_page_size()
{
#if defined(_WIN32)
	SYSTEM_INFO system_info;
	GetSystemInfo(&system_info);
	return system_info.dwPageSize;
#else
	return 4096;
#endif
}

void *reserve(usize size)
{
#if defined(_WIN32)
	void *region = VirtualAlloc(nullptr, size, MEM_RESERVE, PAGE_NOACCESS);
	if (region == nullptr) {
		logger::error("win32 error: {}\n", GetLastError());
		ASSERT(false);
	}
	return region;
#else
	return nullptr;
#endif
}

void *commit(void *page, usize size, MemoryAccess access)
{
	using enum MemoryAccess;

#if defined(_WIN32)
	uint protect = 0;
	if (access == ReadOnly) {
		protect = PAGE_READONLY;
	} else if (access == ReadWrite) {
		protect = PAGE_READWRITE;
	} else {
		ASSERT(false);
	}

	return VirtualAlloc(page, size, MEM_COMMIT, protect);
#else
	return nullptr;
#endif
}

void free(void *region)
{
	if (!region) {
		return;
	}

#if defined(_WIN32)
	auto res = VirtualFree(region, 0, MEM_RELEASE);
	ASSERT(res != 0);
#endif
}
}; // namespace exo::virtual_allocator
