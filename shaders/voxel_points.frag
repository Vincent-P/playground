#version 460
#extension GL_GOOGLE_include_directive : enable

#include "globals.h"
#include "types.h"
#include "voxels.h"
#include "maths.h"

layout(set = 1, binding = 0) uniform VO {
    VoxelOptions voxel_options;
};

layout (set = 1, binding = 1) uniform UBODebug {
    VCTDebug debug;
};

layout(set = 1, binding = 2) uniform sampler3D voxels;

layout(location = 0) out vec4 o_color;

layout(location = 0) flat in int3 i_voxel_pos;
layout(location = 1) flat in float3 i_box_radius;
layout(location = 2) flat in float3 i_box_inv_radius;
layout(location = 3) flat in float4 i_voxel_color;

// vec3 box.radius:       independent half-length along the X, Y, and Z axes
bool ourIntersectBoxCommon(Box box, Ray ray, out float distance, out vec3 normal, in vec3 _invRayDirection) {

    // Move to the box's reference frame. This is unavoidable and un-optimizable.
    ray.origin = (ray.origin - box.center);

    float winding = (max3(abs(ray.origin) * box.invRadius) < 1.0) ? -1.0 : 1.0;

    // We'll use the negated sign of the ray direction in several places, so precompute it.
    // The sign() instruction is fast...but surprisingly not so fast that storing the result
    // temporarily isn't an advantage.
    float3 sgn = -sign(ray.direction);

	// Ray-plane intersection. For each pair of planes, choose the one that is front-facing
    // to the ray and compute the distance to it.
    float3 distanceToPlane = box.radius * winding * sgn - ray.origin;
    distanceToPlane *= _invRayDirection;

    // Perform all three ray-box tests and cast to 0 or 1 on each axis.
    // Use a macro to eliminate the redundant code (no efficiency boost from doing so, of course!)
    // Could be written with
#   define TEST(U, VW)\
         /* Is there a hit on this axis in front of the origin? Use multiplication instead of && for a small speedup */\
         (distanceToPlane.U >= 0.0) && \
         /* Is that hit within the face of the box? */\
         all(lessThan(abs(ray.origin.VW + ray.direction.VW * distanceToPlane.U), box.radius.VW))

    bvec3 test = bvec3(TEST(x, yz), TEST(y, zx), TEST(z, xy));

    // CMOV chain that guarantees exactly one element of sgn is preserved and that the value has the right sign
    sgn = test.x ? vec3(sgn.x, 0.0, 0.0) : (test.y ? vec3(0.0, sgn.y, 0.0) : vec3(0.0, 0.0, test.z ? sgn.z : 0.0));
#   undef TEST

    // At most one element of sgn is non-zero now. That element carries the negative sign of the
    // ray direction as well. Notice that we were able to drop storage of the test vector from registers,
    // because it will never be used again.

    // Mask the distance by the non-zero axis
    // Dot product is faster than this CMOV chain, but doesn't work when distanceToPlane contains nans or infs.
    //
    distance = (sgn.x != 0.0) ? distanceToPlane.x : ((sgn.y != 0.0) ? distanceToPlane.y : distanceToPlane.z);

    // Normal must face back along the ray. If you need
    // to know whether we're entering or leaving the box,
    // then just look at the value of winding. If you need
    // texture coordinates, then use box.invDirection * hitPoint.

    normal = sgn;

    return (sgn.x != 0) || (sgn.y != 0) || (sgn.z != 0);
}

void main()
{
    if (debug.display == 2)
    {
        o_color = float4(i_voxel_color.rgb, 1.0);
        gl_FragDepth = gl_FragCoord.z;
    }
    else
    {
        float3 center = VoxelCenterToWorld(i_voxel_pos, voxel_options);

        float2 pixel_pos = gl_FragCoord.xy;
        float2 uv = pixel_pos / global.resolution;

        float3 clip_space = float3(uv * 2.0 - float2(1.0), 0.0);
        float4 h_pos      = global.camera_inv_view_proj * float4(clip_space, 1.0);
        h_pos /= h_pos.w;

        float3 origin = global.camera_pos;
        float3 direction = normalize(h_pos.xyz - global.camera_pos);
        float3 inv_ray_direction = 1 / direction;

        Box box = {center, i_box_radius, i_box_inv_radius};
        Ray ray = {origin, direction};

        float distance = 0.0;
        float3 normal = float3(0.0);
        if (!ourIntersectBoxCommon(box, ray, distance, normal, inv_ray_direction))
        {
            discard;
            return;
        }

        float3 hit = ray.origin + ray.direction * distance;

        float3 distance_from_center = hit - center;

        // update depth correctly (otherwise it will use screen-aligned quads depth)
        float4 hit_screenspace = global.camera_proj * global.camera_view * float4(hit, 1.0);
        gl_FragDepth = hit_screenspace.z / hit_screenspace.w;

        float distance_coef = length(distance_from_center) / i_box_radius.x;

        const float border_offset = 1.35;
        float3 border = distance_coef > border_offset ? float3(1+border_offset-distance_coef) : float3(1.0);

        // get the voxel color
        o_color = float4(i_voxel_color.rgb * border, 1.0);
    }
}
