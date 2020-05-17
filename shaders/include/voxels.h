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

vec3 EncodeNormal(vec3 normal)
{
    return normal * 0.5f + vec3(0.5f);
}

vec3 DecodeNormal(vec3 normal)
{
    return (normal - vec3(0.5f)) * 2.0f;
}

#endif
