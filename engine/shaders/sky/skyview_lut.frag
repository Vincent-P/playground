#include "types.h"
#include "globals.h"
#include "atmosphere.h"

layout (set = 1, binding = 0) uniform AtmosphereUniform {
    AtmosphereParameters atmosphere;
};

layout(set = 1, binding = 1) uniform sampler2D transmittance_lut; // sampler linear clamp
layout(set = 1, binding = 2) uniform sampler2D multiscattering_lut;

layout(location = 0) out vec4 o_color;

float3 integrate_luminance(AtmosphereParameters atmosphere, float3 p, float3 dir, float3 sun_dir)
{
    float3 throughput = float3(1.0);
    float3 L  = float3(0.0);

    float r    = length(p);
    float3 up = p / r;
    float mu   = dot(up, dir);
    float mu_s = dot(up, sun_dir);
    float v    = dot(dir, sun_dir);

    float mie_phase      = cornette_shanks_phase_function(atmosphere.mie_phase_function_g, -v);
    float rayleigh_phase = rayleigh_phase_function(v);

    const int SAMPLE_COUNT = 40;

    float tBottom = ray_sphere_nearest_intersection(p, dir, float3(0.0), atmosphere.bottom_radius);
    float tTop    = ray_sphere_nearest_intersection(p, dir, float3(0.0), atmosphere.top_radius);
    float tMax    = 0.0f;

    if (tBottom < 0.0f)
    {
        if (tTop < 0.0f)
        {
            tMax = 0.0f; // No intersection with earth nor atmosphere: stop right away
        }
        else
        {
            tMax = tTop;
        }
    }
    else
    {
        if (tTop > 0.0f)
        {
            tMax = min(tTop, tBottom);
        }
    }

    float dx =  tMax / SAMPLE_COUNT;

    for (int i = 0; i <= SAMPLE_COUNT; i++)
    {
        float d = i * dx;
        float rd = safe_sqrt(d * d + r * r + 2 * r * d * mu);
        float mu_s_d = (r * mu_s + d * v) / rd;

        MediumRGB medium = sample_medium(atmosphere, rd);
        float3 sample_transmittance = exp(-medium.extinction * dx);

        float3 sun_transmittance = get_transmittance_to_sun(atmosphere, transmittance_lut, rd, mu_s_d);

        float3 T  = sun_transmittance;
        float  S  = 1.0; // todo?
        float3 p  = medium.mie_scattering * mie_phase + medium.rayleigh_scattering * rayleigh_phase;
        float3 psi_ms = get_multiple_scattering(atmosphere, multiscattering_lut, float3(0.0, rd, 0.0), mu_s_d);
        float3 Ei = global.sun_illuminance;

        // L_scat(c,x,v) = σs(x) ∑_{i=1}^{N_light} (T(c,x) S(x,li) p(v,li) + Ψms) Ei   (11)
        float3 Lscat = Ei * (T * S * p + psi_ms * medium.scattering);

        /// --- Integrate scattering

        // Integration formula, see slide 28 at http://www.frostbite.com/2015/08/physically-based-unified-volumetric-rendering-in-frostbite/
        // integrate along the current step segment
        float3 L_int = (Lscat - Lscat * sample_transmittance) / medium.extinction;
        // accumulate and also take into account the transmittance from previous steps
        L += throughput * L_int;

        throughput *= sample_transmittance;
    }

    return L;
}

void main()
{
    const float2 LUT_SIZE = float2(192.0,108.0); // TODO: uniform?
    float2 pixel_pos = gl_FragCoord.xy;
    float2 uv = pixel_pos / LUT_SIZE;

    float3 clip_space;
    clip_space.xy = uv * 2.0 - float2(1.0);
    clip_space.z = 1;
    float4 h_pos = global.camera_inv_view_proj * float4(clip_space, 1.0);
    h_pos.xyz /= h_pos.w;

    float3 world_dir = normalize(h_pos.xyz - global.camera_pos);
    float3 world_pos = global.camera_pos + float3(0.0, atmosphere.bottom_radius, 0.0);

    float r = length(world_pos);
    float mu;
    float cos_lightview;
    uv_to_mu_coslightview(atmosphere, uv, r, mu, cos_lightview);

    float3 up = world_pos / r;
    float mu_s = dot(up, global.sun_direction);
    float3 sun_dir = normalize(float3(safe_sqrt(1.0 - mu_s * mu_s), mu_s, 0.0));

    world_pos = float3(0.0, r, 0.0);

    float sin_theta = safe_sqrt(1.0 - mu * mu);

    world_dir = float3(
        sin_theta * cos_lightview,
        mu,
        sin_theta * safe_sqrt(1.0 - cos_lightview * cos_lightview)
        );


    if (!move_to_top_atmosphere(atmosphere, world_pos, world_dir))
    {
        // Ray is not intersecting the atmosphere
        o_color = float4(1, 0, 0, 1);
        return;
    }

    float3 L = integrate_luminance(atmosphere, world_pos, world_dir, sun_dir);
    o_color = float4(L, 1.0);
}
