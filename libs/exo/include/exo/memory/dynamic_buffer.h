#pragma once
#include <exo/collections/span.h>
#include <exo/maths/numerics.h>

namespace exo
{
struct DynamicBuffer
{
	void *ptr  = nullptr;
	usize size = 0;

	static void init(DynamicBuffer &buffer, usize new_size);
	void        destroy();
	Span<u8>    content() { return Span<u8>(static_cast<u8 *>(ptr), size); }

	void resize(usize new_size);
};
} // namespace exo
