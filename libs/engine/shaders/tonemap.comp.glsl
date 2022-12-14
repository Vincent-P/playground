#pragma shader_stage(compute)

#include "base/types.h"
#include "base/constants.h"
#include "engine/globals.h"

layout(set = SHADER_UNIFORM_SET, binding = 0) uniform Options {
    uint sampled_hdr_buffer;
    uint storage_output_frame;
};

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
void main()
{
    uint local_idx   = gl_LocalInvocationIndex;
    uint3 global_idx = gl_GlobalInvocationID;
    uint3 group_idx  = gl_WorkGroupID;

    int2 pixel_pos = int2(global_idx.xy);
    int2 output_size = imageSize(global_images_2d_rgba8[storage_output_frame]);

    if (any(greaterThan(pixel_pos, output_size)))
    {
        return;
    }

    float3 hdr = texelFetch(global_textures[sampled_hdr_buffer], pixel_pos, LOD0).rgb;

    float3 ldr = hdr;

    // to srgb
    ldr = pow(ldr, float3(1.0 / 2.2));


    float4 output_color = vec4(ldr, 1.0);
    imageStore(global_images_2d_rgba8[storage_output_frame], pixel_pos, output_color);
}
