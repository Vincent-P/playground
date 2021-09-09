// -*- mode: glsl; -*-

#ifndef RAYTRACING_H
#define RAYTRACING_H

#include "types.h"
#include "maths.h"
#include "constants.h"

// -- Structs

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
    float t_min;
    float3 direction;
    float t_max;
};

// -- Intersection functions

bool fast_box_intersection(float3 box_min, float3 box_max, Ray ray, float3 inv_ray_dir)
{
  float3 t0 = (box_min - ray.origin) * inv_ray_dir;
  float3 t1 = (box_max - ray.origin) * inv_ray_dir;
  float tmin = max(max3(min(t0,t1)), ray.t_min);
  float tmax = min(min3(max(t0,t1)), ray.t_max);
  return tmin <= tmax;
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
