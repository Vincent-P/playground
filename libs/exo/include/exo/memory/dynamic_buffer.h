#pragma once
#include <exo/collections/span.h>
#include <exo/maths/numerics.h>

namespace exo
{
struct DynamicBuffer
{
	void *ptr  = nullptr;
	usize size = 0;

	// --

	DynamicBuffer() = default;

	DynamicBuffer(const DynamicBuffer &copy)            = delete;
	DynamicBuffer &operator=(const DynamicBuffer &copy) = delete;

	DynamicBuffer(DynamicBuffer &&moved) noexcept
	{
		this->ptr  = moved.ptr;
		this->size = moved.size;
		moved.ptr  = nullptr;
		moved.size = 0;
	}

	DynamicBuffer &operator=(DynamicBuffer &&moved) noexcept
	{
		this->ptr  = moved.ptr;
		this->size = moved.size;
		moved.ptr  = nullptr;
		moved.size = 0;
		return *this;
	}

	static void init(DynamicBuffer &buffer, usize new_size);
	void        destroy();

	Span<u8>       content() { return Span<u8>(static_cast<u8 *>(ptr), size); }
	Span<const u8> content() const { return Span<const u8>(static_cast<const u8 *>(ptr), size); }

	void resize(usize new_size);
};
} // namespace exo
