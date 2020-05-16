#version 450

layout (location = 0) in vec3 inWorldPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV0;
layout (location = 3) in vec2 inUV1;
layout (location = 4) in vec3 inVoxelPos;

layout(set = 1, binding = 0, r32ui) uniform uimage3D voxels_texture;

layout(set = 1, binding = 1) uniform UBO {
    vec3 center;
    float size;
    uint res;
} debug_options;

layout(set = 2, binding = 1) uniform sampler2D baseColorTexture;
layout(set = 2, binding = 2) uniform sampler2D normalTexture;

layout (push_constant) uniform Material
{
    vec4 baseColorFactor;
    vec4 emissiveFactor;
    vec4 diffuseFactor;
    vec4 specularFactor;
    float workflow;
    int baseColorTextureSet;
    int physicalDescriptorTextureSet;
    int normalTextureSet;
    int occlusionTextureSet;
    int emissiveTextureSet;
    float metallicFactor;
    float roughnessFactor;
    float alphaMask;
    float alphaMaskCutoff;
} material;

void imageAtomicAverageRGBA8(ivec3 coord, vec3 nextVec3)
{
    uint nextUint = packUnorm4x8(vec4(nextVec3,1.0f/255.0f));
    uint prevUint = 0;
    uint currUint;

    vec4 currVec4;

    vec3 average;
    uint count;

    //"Spin" while threads are trying to change the voxel
    while((currUint = imageAtomicCompSwap(voxels_texture, coord, prevUint, nextUint)) != prevUint)
    {
        prevUint = currUint;                    //store packed rgb average and count
        currVec4 = unpackUnorm4x8(currUint);    //unpack stored rgb average and count

        average =      currVec4.rgb;        //extract rgb average
        count   = uint(currVec4.a*255.0f);  //extract count

        //Compute the running average
        average = (average*count + nextVec3) / (count+1);

        //Pack new average and incremented count back into a uint
        nextUint = packUnorm4x8(vec4(average, (count+1)/255.0f));
    }
}

vec3 getNormal()
{
    // Perturb normal, see http://www.thetenthplanet.de/archives/1180
    vec3 tangentNormal = texture(normalTexture, inUV0).xyz * 2.0 - 1.0;

    vec3 q1 = dFdx(inWorldPos);
    vec3 q2 = dFdy(inWorldPos);
    vec2 st1 = dFdx(inUV0);
    vec2 st2 = dFdy(inUV0);

    vec3 N = normalize(inNormal);
    vec3 T = normalize(q1 * st2.t - q2 * st1.t);
    vec3 B = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}


void main()
{
    vec4 color = vec4(getNormal(), 1);
    color = texture(baseColorTexture, inUV0);
    if (color.a < 0.1) {
        discard;
    }

    // output:
    ivec3 pos = ivec3(floor(inVoxelPos));
    imageAtomicAverageRGBA8(pos, color.rgb);
}
