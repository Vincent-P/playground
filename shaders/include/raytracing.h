#ifndef RAYTRACING_H
#define RAYTRACING_H

#include "types.h"
#include "maths.h"
#include "constants.h"

struct Box
{
    float3 center;
    float3 radius;
    float3 inv_radius;
};

struct Sphere
{
    float3 center;
    float radius;
};

struct Triangle
{
    vec3 v0;
    vec3 e0;
    vec3 e1;
};

struct Ray
{
    float3 origin;
    /** Unit direction of propagation */
    float3 direction;
};

// -- Intersection functions

bool fast_box_intersection(float3 box_min, float3 box_max, Ray ray, float3 inv_ray_dir)
{
  float3 t0 = (box_min - ray.origin) * inv_ray_dir;
  float3 t1 = (box_max - ray.origin) * inv_ray_dir;
  float3 tmin = min(t0,t1);
  float3 tmax = max(t0,t1);
  return max3(tmin) <= min3(tmax);
}

// vec3 box.radius:       independent half-length along the X, Y, and Z axes
bool ray_box_intersection(Box box, Ray ray, out float distance, out float3 normal, in float3 _invRayDirection) {

    // Move to the box's reference frame. This is unavoidable and un-optimizable.
    ray.origin = (ray.origin - box.center);

    float winding = (max3(abs(ray.origin) * box.inv_radius) < 1.0) ? -1.0 : 1.0;

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


bool ray_sphere_nearest_intersection(Ray ray, Sphere sphere, out float d, out float3 normal)
{
    float a = dot(ray.direction, ray.direction);

    float3 s0_r0 = ray.origin - sphere.center;

    float b = 2.0 * dot(ray.direction, s0_r0);
    float c = dot(s0_r0, s0_r0) - (sphere.radius * sphere.radius);
    float delta = b * b - 4.0*a*c;
    if (delta < 0.0 || a == 0.0)
    {
        return false;
    }
    float sol0 = (-b - sqrt(delta)) / (2.0*a);
    float sol1 = (-b + sqrt(delta)) / (2.0*a);
    if (sol0 < 0.0 && sol1 < 0.0)
    {
        return false;
    }

    if (sol0 < 0.0)
    {
        d = max(0.0, sol1);
    }
    else if (sol1 < 0.0)
    {
        d = max(0.0, sol0);
    }
    else
    {
        d = max(0.0, min(sol0, sol1));
    }

    normal = normalize(ray.origin + d * ray.direction - sphere.center);

    return true;
}

vec3 triangle_intersection(Ray ray, Triangle tri, out float o_d)
{
    vec3 rov0 = ray.origin - tri.v0;
    vec3  n = cross( tri.e0, tri.e1 );
    vec3  q = cross( rov0, ray.direction );
    float d = 1.0/dot( ray.direction, n );
    float u = d*dot( -q, tri.e1 );
    float v = d*dot(  q, tri.e0 );
    o_d = d*dot( -n, rov0 );
    if( u<0.0 || u>1.0 || v<0.0 || (u+v)>1.0 ) o_d = -1.0;
    return vec3( 1.0 - u - v, u, v );
}

#endif
