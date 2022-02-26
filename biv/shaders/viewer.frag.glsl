#pragma shader_stage(fragment)

#include "base/types.h"
#include "biv/globals.h"

layout(set = SHADER_UNIFORM_SET, binding = 0) uniform Options {
    float2 scale;
    float2 translation;
    u32 texture_descriptor;
    u32 viewer_flags;
};

layout(location = 0) in float2 i_uv;
layout(location = 0) out float4 o_color;
void main()
{
	float4 result = float4(0.0);

	// Sample the texture
    float4 s = texture(global_textures[texture_descriptor], i_uv);
	result = s;

	// Set unchecked channels to 0
	u32 display_channels = viewer_flags & 0xF;
	if ((display_channels & 8u) == 0)
	{
		result.r = 0.0;
	}
	if ((display_channels & 4u) == 0)
	{
		result.g = 0.0;
	}
	if ((display_channels & 2u) == 0)
	{
		result.b = 0.0;
	}
	if ((display_channels & 1u) == 0)
	{
		result.a = 1.0;
	}

	// If there is only one channel checked display it in greyscale
	if (bitCount(display_channels) == 1)
	{
		// display channels is laid out as RGBA but that makes A bit 0, B bit 1, G bit 2, R bit 3
		float value = result[3-findLSB(display_channels)];
		result = float4(value, value, value, 1.0);
	}

	o_color = float4(result);
}
