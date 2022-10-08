#pragma once
#include <exo/maths/numerics.h>

namespace exo
{
struct DynamicBuffer
{
	void *ptr  = nullptr;
	usize size = 0;

	static void init(DynamicBuffer &buffer, usize new_size);
	void        destroy();

	void resize(usize new_size);
};
} // namespace exo
