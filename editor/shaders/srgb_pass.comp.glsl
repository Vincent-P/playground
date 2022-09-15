#pragma shader_stage(compute)

#include "base/types.h"
#include "base/constants.h"
#include "base/bindless.h"

layout(set = SHADER_UNIFORM_SET, binding = 0) uniform Options {
    u32 linear_input_buffer_texture;
    u32 srgb_output_buffer_image;
};

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
void main()
{
    uint local_idx   = gl_LocalInvocationIndex;
    uint3 global_idx = gl_GlobalInvocationID;
    uint3 group_idx  = gl_WorkGroupID;

    int2 pixel_pos = int2(global_idx.xy);
    int2 output_size = imageSize(global_images_2d_rgba8[srgb_output_buffer_image]);

    if (any(greaterThan(pixel_pos, output_size)))
    {
        return;
    }

    pixel_pos = int2(0, 0);
    
    float4 output_color = vec4(1, 0, 0, 1.0);
    imageStore(global_images_2d_rgba8[srgb_output_buffer_image], pixel_pos, output_color);
    return;
    #if 0

    float3 hdr = texelFetch(global_textures[linear_input_buffer_texture], pixel_pos, LOD0).rgb;

    float3 ldr = hdr;

    // to srgb
    ldr = pow(ldr, float3(1.0 / 2.2));


    float4 output_color = vec4(ldr, 1.0);
    imageStore(global_images_2d_rgba8[srgb_output_buffer_image], pixel_pos, output_color);
    #endif
}
