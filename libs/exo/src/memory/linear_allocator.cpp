#include "exo/memory/linear_allocator.h"

#include "exo/macros/assert.h"
#include "exo/maths/pointer.h"
#include <utility>

namespace exo
{
LinearAllocator LinearAllocator::with_external_memory(void *p, usize len)
{
	LinearAllocator result = {};
	result.base_address    = reinterpret_cast<u8 *>(p);
	result.ptr             = result.base_address;
	result.end             = result.ptr + len;
	return result;
}

LinearAllocator::LinearAllocator(LinearAllocator &&other) noexcept { *this = std::move(other); }

LinearAllocator &LinearAllocator::operator=(LinearAllocator &&other) noexcept
{
	this->base_address = std::exchange(other.base_address, nullptr);
	this->ptr          = std::exchange(other.ptr, nullptr);
	this->end          = std::exchange(other.end, nullptr);
	return *this;
}

void *LinearAllocator::allocate(usize size)
{
	size       = round_up_to_alignment(sizeof(u32), size);
	u8 *result = this->ptr;
	ASSERT(result + size < this->end);
	this->ptr = this->ptr + size;
	return result;
}

void LinearAllocator::rewind(void *p) { this->ptr = reinterpret_cast<u8 *>(p); }
} // namespace exo
