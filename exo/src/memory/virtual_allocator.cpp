#include "exo/memory/virtual_allocator.h"

#include "exo/logger.h"
#include "exo/macros/assert.h"
#include <windows.h>

namespace exo::virtual_allocator
{
u32 get_page_size()
{
    SYSTEM_INFO system_info;
    GetSystemInfo(&system_info);
    return system_info.dwPageSize;
}

void *reserve(usize size)
{
    void *region = VirtualAlloc(nullptr, size, MEM_RESERVE, PAGE_NOACCESS);
    if (region == nullptr) {
        logger::error("win32 error: {}\n", GetLastError());
        ASSERT(false);
    }
    return region;
}

void *commit(void *page, usize size, MemoryAccess access)
{
    using enum MemoryAccess;

    uint protect = 0;
    if (access == ReadOnly)
    {
        protect = PAGE_READONLY;
    }
    else if (access == ReadWrite)
    {
        protect = PAGE_READWRITE;
    }
    else
    {
        ASSERT(false);
    }

    return VirtualAlloc(page, size, MEM_COMMIT, protect);
}

void free(void *region)
{
    if (!region)
    {
        return;
    }
    auto res = VirtualFree(region, 0, MEM_RELEASE);
    ASSERT(res != 0);
}
}; // namespace virtual_allocator
