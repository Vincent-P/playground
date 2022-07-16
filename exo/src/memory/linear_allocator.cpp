#include "exo/memory/linear_allocator.h"

#include "exo/macros/assert.h"
#include "exo/maths/pointer.h"
#include <utility>

namespace exo
{

LinearAllocator LinearAllocator::with_external_memory(void *p, usize len)
{
	LinearAllocator result = {};
	result.ptr             = reinterpret_cast<u8 *>(p);
	result.end             = result.ptr + len;
	return result;
}

LinearAllocator::LinearAllocator(LinearAllocator &&other) { *this = std::move(other); }

LinearAllocator &LinearAllocator::operator=(LinearAllocator &&other)
{
	this->ptr = std::exchange(other.ptr, nullptr);
	this->end = std::exchange(other.end, nullptr);
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
