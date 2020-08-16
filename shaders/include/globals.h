#include "types.h"

layout (set = 0, binding = 0) uniform GlobalUniform {
    float3 camera_pos;
    float pad10;

    float4x4 camera_view;
    float4x4 camera_proj;
    float4x4 camera_inv_proj;
    float4x4 sun_view;
    float4x4 sun_proj;

    // to add
    uint2 resolution;
    float2 raymarch_min_max_spp;
    float MultipleScatteringFactor;
    float MultiScatteringLUTRes;

    int TRANSMITTANCE_TEXTURE_WIDTH;
    int TRANSMITTANCE_TEXTURE_HEIGHT;

    float3 sun_direction;
    float pad000;

    float3 sun_illuminance;
    float pad1234;

    //
    // From AtmosphereParameters
    //
    float3	solar_irradiance;
    float	sun_angular_radius;

    float3	absorption_extinction;
    float	mu_s_min;

    float3	rayleigh_scattering;
    float	mie_phase_function_g;

    float3	mie_scattering;
    float	bottom_radius;

    float3	mie_extinction;
    float	top_radius;

    float3	mie_absorption;
    float	pad00;

    float3	ground_albedo;
    float       pad0;

    float4 rayleigh_density[3];
    float4 mie_density[3];
    float4 absorption_density[3];
} global;
