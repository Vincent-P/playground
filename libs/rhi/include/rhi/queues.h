#pragma once
#include "exo/maths/numerics.h"

namespace rhi
{
enum struct QueueType : u8
{
	Graphics = 0,
	Compute  = 1,
	Transfer = 2,
	Count
};
}
