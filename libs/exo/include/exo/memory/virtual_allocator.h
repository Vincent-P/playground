#pragma once
#include <exo/maths/numerics.h>

namespace exo::virtual_allocator
{
enum struct MemoryAccess
{
	ReadOnly,
	ReadWrite
};

u32   get_page_size();
void *reserve(usize size);
void *commit(void *page, usize size, MemoryAccess access = MemoryAccess::ReadWrite);
void  free(void *region);
}; // namespace exo::virtual_allocator
