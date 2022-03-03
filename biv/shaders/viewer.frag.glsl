#pragma shader_stage(fragment)

#include "base/types.h"
#include "biv/globals.h"

layout(set = SHADER_UNIFORM_SET, binding = 0) uniform Options {
    float2 scale;
    float2 translation;
    u32 texture_descriptor;
    u32 viewer_flags;
	float2 viewport_size;
};

layout(location = 0) in float2 i_uv;
layout(location = 0) out float4 o_color;
void main()
{
	float4 result = float4(0.0);

	// Sample the texture
	float2 texture_size = textureSize(global_textures[texture_descriptor], LOD0);
	float2 texture_texel_size = 1.0 / texture_size;
	float2 viewport_texel_size = 1.0 / viewport_size;
	
	// grid spacing
	// 1-10
	// 10-50
	// 20-100
	// 100
	float grid_spacing = 100.0;
	if (all(greaterThan(20.0 * texture_texel_size, 50.0 * viewport_texel_size)))
	{
		grid_spacing = 20.0;
	}
	else if (all(greaterThan(10.0 * texture_texel_size, 5.0 * viewport_texel_size)))
	{
		grid_spacing = 10.0;
	}
	else if (all(greaterThan(texture_texel_size, 5.0 * viewport_texel_size)))
	{
		grid_spacing = 1.0;
	}

	float2 grid_coord = i_uv * texture_size / grid_spacing;
	float2 grid_delta = abs(fract(grid_coord - 0.5) - 0.5) / fwidth(grid_coord);
	float grid_line   = min(grid_delta.x, grid_delta.y);
	float3 grid_color = float3(1.0 - min(grid_line, 1.0));

    float4 texture_sample = texture(global_textures[texture_descriptor], i_uv);
	result = texture_sample;

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

	result.rgb += grid_color * 0.1;

	o_color = float4(result);
}
