#include "exo/memory/dynamic_buffer.h"

#include "exo/macros/assert.h"
#include "exo/profile.h"

#include <cstdlib> // for calloc, realloc, free

namespace exo
{
void DynamicBuffer::init(DynamicBuffer &buffer, usize new_size)
{
	ASSERT(buffer.size == 0);
	ASSERT(buffer.ptr == nullptr);

	buffer.size = new_size;
	buffer.ptr  = calloc(1, new_size);
	EXO_PROFILE_MALLOC(buffer.ptr, buffer.size);

	ASSERT(buffer.size > 0);
	ASSERT(buffer.ptr != nullptr);
}

void DynamicBuffer::destroy()
{
	free(this->ptr);
	EXO_PROFILE_MFREE(this->ptr);

	this->ptr  = nullptr;
	this->size = 0;
}

void DynamicBuffer::resize(usize new_size)
{
	void *new_buffer = realloc(this->ptr, new_size);
	ASSERT(new_buffer);

	EXO_PROFILE_MFREE(this->ptr);
	EXO_PROFILE_MALLOC(new_buffer, new_size);

	this->ptr  = new_buffer;
	this->size = new_size;
}
} // namespace exo
