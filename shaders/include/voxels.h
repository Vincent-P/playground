#ifndef VOXELS_H
#define VOXELS_H

struct VoxelOptions
{
    vec3 center;
    float size;
    uint res;
};

// Compute the running average of a 3D uint texture
// Without it, the values will change it frame depending on the execution order

/// void imageAtomicAverageRGBA8(layout(r32ui) uimage3D voxels, ivec3 coord, vec3 value)
#define imageAtomicAverageRGBA8(voxels, coord, value)                                          \
    {                                                                                          \
        uint nextUint = packUnorm4x8(vec4(value,1.0f/255.0f));                                 \
        uint prevUint = 0;                                                                     \
        uint currUint;                                                                         \
                                                                                               \
        vec4 currVec4;                                                                         \
                                                                                               \
        vec3 average;                                                                          \
        uint count;                                                                            \
                                                                                               \
        /*"Spin" while threads are trying to change the voxel*/                                \
        while((currUint = imageAtomicCompSwap(voxels, coord, prevUint, nextUint)) != prevUint) \
        {                                                                                      \
            prevUint = currUint;                   /*store packed rgb average and count*/      \
            currVec4 = unpackUnorm4x8(currUint);   /*unpack stored rgb average and count*/     \
                                                                                               \
            average =      currVec4.rgb;        /*extract rgb average*/                        \
            count   = uint(currVec4.a*255.0f);  /*extract count*/                              \
                                                                                               \
            /*Compute the running average*/                                                    \
            average = (average*count + value) / (count+1);                                     \
                                                                                               \
            /*Pack new average and incremented count back into a uint*/                        \
            nextUint = packUnorm4x8(vec4(average, (count+1)/255.0f));                          \
        }                                                                                      \
    }

ivec3 WorldToVoxel(vec3 world_pos, VoxelOptions options)
{
    vec3 voxel_pos = (world_pos - floor(options.center)) / options.size;
    return ivec3(floor(voxel_pos));
}

vec3 WorldToVoxelTex(vec3 world_pos, VoxelOptions options)
{
    vec3 voxel_pos = (world_pos - floor(options.center)) / options.size;
    return voxel_pos / options.res;
}

vec3 EncodeNormal(vec3 normal)
{
    return normal * 0.5f + vec3(0.5f);
}

vec3 DecodeNormal(vec3 normal)
{
    return (normal - vec3(0.5f)) * 2.0f;
}

const ivec3 g_aniso_offsets[] = ivec3[8]
    (
        ivec3(0, 0, 0),
        ivec3(0, 0, 1),
        ivec3(0, 1, 0),
        ivec3(0, 1, 1),
        ivec3(1, 0, 0),
        ivec3(1, 0, 1),
        ivec3(1, 1, 0),
        ivec3(1, 1, 1)
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
