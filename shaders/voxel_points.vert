#version 460
#extension GL_GOOGLE_include_directive : enable

#include "types.h"
#include "globals.h"
#include "voxels.h"

layout(set = 1, binding = 0) uniform VO {
    VoxelOptions voxel_options;
};

layout (set = 1, binding = 1) uniform UBODebug {
    VCTDebug debug;
};

layout(set = 1, binding = 2) uniform sampler3D voxels;

layout(location = 0) flat out int3   o_voxel_pos;
layout(location = 1) flat out float3 o_box_radius;
layout(location = 2) flat out float3 o_box_inv_radius;
layout(location = 3) flat out float4 o_voxel_color;

void quadricProj (in vec3 osPosition, in float voxelSize, in mat4 objectToScreenMatrix, in vec2 halfScreenSize,inout vec4 position, inout float pointSize)
{
    const vec4 quadricMat = vec4(1.0, 1.0, 1.0, -1.0);
    float sphereRadius = voxelSize * 1.732051;
    vec4 sphereCenter = vec4(osPosition.xyz, 1.0);
    mat4 modelViewProj = transpose(objectToScreenMatrix);

    mat3x4 matT = mat3x4( mat3(modelViewProj[0].xyz, modelViewProj[1].xyz, modelViewProj[3].xyz)*sphereRadius);
    matT[0].w = dot(sphereCenter, modelViewProj[0]);
    matT[1].w = dot(sphereCenter, modelViewProj[1]);
    matT[2].w = dot(sphereCenter, modelViewProj[3]);

    mat3x4 matD  = mat3x4(matT[0]*quadricMat, matT[1]*quadricMat, matT[2]*quadricMat);
    vec4 eqCoefs = vec4(dot(matD[0], matT[2]),
                        dot(matD[1], matT[2]),
                        dot(matD[0], matT[0]),
                        dot(matD[1], matT[1]))
                / dot(matD[2], matT[2]);

    vec4 outPosition = vec4(eqCoefs.x, eqCoefs.y, 0.0, 1.0);
    vec2 AABB = sqrt(eqCoefs.xy*eqCoefs.xy - eqCoefs.zw);
    AABB *= halfScreenSize*2.0f;

    position.xy = outPosition.xy*position.w;
    pointSize = max(AABB.x, AABB.y);
}

void main()
{
    gl_PointSize = 0.0;

    // get the voxel index from the vertex / instance
    uint remaining = gl_VertexIndex / voxel_options.res;
    int3 voxel_pos = int3(
        int(gl_VertexIndex % voxel_options.res),
        int(remaining % voxel_options.res),
        int(remaining / voxel_options.res)
    );

    o_voxel_pos = voxel_pos;
    o_box_radius = float3(0.0);

    // cull empty voxels
    float4 voxel = texelFetch(voxels, voxel_pos, debug.voxel_selected_mip);
    o_voxel_color = voxel;
    if (voxel.a <= 0.001) {
        gl_Position = float4(-1.0);
        return;
    }

    // compute screenspace bounds of the voxel
    float3 world_pos = VoxelCenterToWorld(voxel_pos, voxel_options);
    float4 out_pos = global.camera_proj * global.camera_view * float4(world_pos, 1.0);
    float out_point_size = 1.0;

    quadricProj(world_pos,
                voxel_options.size / 2,
                global.camera_proj * global.camera_view,
                float2(global.resolution / 2),
                out_pos, out_point_size);

    gl_Position = out_pos / out_pos.w;
    gl_PointSize = out_point_size;

    // cull near plane
    if (gl_Position.z >= 1.0) {
        gl_Position = float4(-1);
        return;
    }

    // stochastically cull subpixel bounds
    float stochasticCoverage = gl_PointSize*gl_PointSize;
    if ((stochasticCoverage < 0.8) &&((gl_VertexIndex & 0xffff) > stochasticCoverage * (0xffff / 0.8))) {
        //"Cull" small voxels in a stable, stochastic way by moving past the z = 0 plane.
        //Assumes voxels are in randomized order.
        gl_Position = float4(-1);
        return;
    }

    o_box_radius = float3(voxel_options.size / 2);
    o_box_inv_radius = 1 / o_box_radius;
}
