#ifndef VOXELS_H
#define VOXELS_H

#include "types.h"

struct VoxelOptions
{
    float3 center;
    float size;
    uint res;
};

struct VCTDebug
{
    uint display; // 0: glTF 1: voxels 2: custom
    uint display_selected; // Voxels (0: albedo 1: normal 2: radiance) glTF (0: nothing 1: base color 2: normal 3: ao 4: indirect lighting)
    int  voxel_selected_mip;
    uint padding00;

    // cone tracing
    float trace_dist;
    float occlusion_lambda;
    float sampling_factor;
    float start;

    // voxel direct lighting
    float4 point_position;
    float point_scale;
    float trace_shadow_hit;
    float max_dist;
    float first_step;
};


// Compute the running average of a 3D uint texture
// Without it, the values will change it frame depending on the execution order

/// void imageAtomicAverageRGBA8(layout(r32ui) uimage3D voxels, int3 coord, float3 value)
#define imageAtomicAverageRGBA8(voxels, coord, value)                                          \
    {                                                                                          \
        uint nextUint = packUnorm4x8(float4(value,1.0f/255.0f));                                 \
        uint prevUint = 0;                                                                     \
        uint currUint;                                                                         \
                                                                                               \
        float4 currFloat4;                                                                         \
                                                                                               \
        float3 average;                                                                          \
        uint count;                                                                            \
                                                                                               \
        /*"Spin" while threads are trying to change the voxel*/                                \
        while((currUint = imageAtomicCompSwap(voxels, coord, prevUint, nextUint)) != prevUint) \
        {                                                                                      \
            prevUint = currUint;                   /*store packed rgb average and count*/      \
            currFloat4 = unpackUnorm4x8(currUint);   /*unpack stored rgb average and count*/     \
                                                                                               \
            average =      currFloat4.rgb;        /*extract rgb average*/                        \
            count   = uint(currFloat4.a*255.0f);  /*extract count*/                              \
                                                                                               \
            /*Compute the running average*/                                                    \
            average = (average*count + value) / (count+1);                                \
                                                                                               \
            /*Pack new average and incremented count back into a uint*/                        \
            nextUint = packUnorm4x8(float4(average, (count+1)/255.0f));                          \
        }                                                                                      \
    }

int3 WorldToVoxel(float3 world_pos, VoxelOptions options)
{
    float3 voxel_pos = (world_pos - options.center) / options.size;
    return int3(floor(voxel_pos));
}

float3 WorldToVoxelTex(float3 world_pos, VoxelOptions options)
{
    float3 voxel_pos = (world_pos - options.center) / options.size;
    return voxel_pos / float(options.res);
}

float3 VoxelToWorld(int3 voxel_pos, VoxelOptions options)
{
    return (options.size * float3(voxel_pos)) + options.center;
}

float3 VoxelCenterToWorld(int3 voxel_pos, VoxelOptions options)
{
    return (options.size * float3(voxel_pos)) + options.center + float3(options.size);
}

float3 EncodeNormal(float3 normal)
{
    return normal * 0.5f + float3(0.5f);
}

float3 DecodeNormal(float3 normal)
{
    return (normal - float3(0.5f)) * 2.0f;
}

const int3 g_aniso_offsets[] = int3[8]
    (
        int3(0, 0, 0),
        int3(0, 0, 1),
        int3(0, 1, 0),
        int3(0, 1, 1),
        int3(1, 0, 0),
        int3(1, 0, 1),
        int3(1, 1, 0),
        int3(1, 1, 1)
    );

#define AnisoTexelFetch(texture, mip, pos)                                   \
    {                                                                        \
        values[0] = texelFetchOffset(texture, pos , mip, g_aniso_offsets[0]);\
        values[1] = texelFetchOffset(texture, pos , mip, g_aniso_offsets[1]);\
        values[2] = texelFetchOffset(texture, pos , mip, g_aniso_offsets[2]);\
        values[3] = texelFetchOffset(texture, pos , mip, g_aniso_offsets[3]);\
        values[4] = texelFetchOffset(texture, pos , mip, g_aniso_offsets[4]);\
        values[5] = texelFetchOffset(texture, pos , mip, g_aniso_offsets[5]);\
        values[6] = texelFetchOffset(texture, pos , mip, g_aniso_offsets[6]);\
        values[7] = texelFetchOffset(texture, pos , mip, g_aniso_offsets[7]);\
    }

#define AnisoImageLoad(texture, pos)                                    \
    {                                                                   \
        values[0] = imageLoad(texture, pos + g_aniso_offsets[0]);\
        values[1] = imageLoad(texture, pos + g_aniso_offsets[1]);\
        values[2] = imageLoad(texture, pos + g_aniso_offsets[2]);\
        values[3] = imageLoad(texture, pos + g_aniso_offsets[3]);\
        values[4] = imageLoad(texture, pos + g_aniso_offsets[4]);\
        values[5] = imageLoad(texture, pos + g_aniso_offsets[5]);\
        values[6] = imageLoad(texture, pos + g_aniso_offsets[6]);\
        values[7] = imageLoad(texture, pos + g_aniso_offsets[7]);\
    }

#endif
