#pragma once
#include <exo/collections/vector.h>
#include <exo/maths/numerics.h>

#include <string>
#include <volk.h>

namespace vulkan
{
struct Shader
{
	std::string    filename;
	VkShaderModule vkhandle;
	Vec<u8>        bytecode;
	bool           operator==(const Shader &other) const = default;
};
} // namespace vulkan
