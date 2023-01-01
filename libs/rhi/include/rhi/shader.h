#pragma once
#include "exo/collections/vector.h"
#include "exo/maths/numerics.h"

#include "exo/string.h"
#include <volk.h>

namespace rhi
{
struct Shader
{
	exo::String    filename;
	VkShaderModule vkhandle;
	Vec<u8>        bytecode;
	bool           operator==(const Shader &other) const = default;
};
} // namespace rhi
