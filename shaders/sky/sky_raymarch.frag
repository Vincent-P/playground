#include "types.h"
#include "globals.h"
#include "atmosphere.h"

layout (set = 1, binding = 0) uniform AtmosphereUniform {
    AtmosphereParameters atmosphere;
};

layout(set = 1, binding = 1) uniform sampler2D transmittance_lut;
layout(set = 1, binding = 2) uniform sampler2D skyview_lut;
layout(set = 1, binding = 3) uniform sampler2D depth_buffer;
layout(set = 1, binding = 4) uniform sampler2D multiscattering_lut;

layout(location = 0) out vec4 o_color;

void integrate_luminance(AtmosphereParameters atmosphere, out float3 L_out, out float3 T_out, float3 p, float3 dir, float3 sun_dir, float max_distance)
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

    const int SAMPLE_COUNT = 5;

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

    float dx =   min(tMax, max_distance)/ SAMPLE_COUNT;

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

    L_out = L;
    T_out = throughput;
}


void main()
{
    float2 pixel_pos = gl_FragCoord.xy;
    float2 uv = pixel_pos / global.resolution;

    float3 clip_space = float3(uv * 2.0 - float2(1.0), 0.0);
    float4 h_pos      = global.camera_inv_view_proj * float4(clip_space, 1.0);
    h_pos /= h_pos.w;

    float3 world_dir = normalize(h_pos.xyz - global.camera_pos);
    float3 world_pos = global.camera_pos + float3(0.0, atmosphere.bottom_radius, 0.0);

    world_pos.y = max(world_pos.y, atmosphere.bottom_radius);

    float r = length(world_pos);
    float3 up = normalize(world_pos);

    float depth = texelFetch(depth_buffer, int2(pixel_pos), 0).r;
    float mu = dot(up, world_dir);

    float3 L_sun = get_sun_luminance(world_pos, world_dir, atmosphere.bottom_radius);

    if (r < atmosphere.top_radius && depth <= 0.0000001)
    {
        float cos_lightview;

        float3 side = normalize(cross(up, world_dir));		// assumes non parallel vectors
        float3 forward = normalize(cross(side, up));	// aligns toward the sun light but perpendicular to up vector
        float2 light_on_plane = float2(dot(global.sun_direction, forward), dot(global.sun_direction, side));
        light_on_plane = normalize(light_on_plane);
        cos_lightview = light_on_plane.x;

        bool intersects_ground = intersects_ground(atmosphere, r, mu);
        float2 skyview_uv = mu_coslightview_to_uv(atmosphere, intersects_ground, r, mu, cos_lightview);

        float3 L = textureLod(skyview_lut, skyview_uv, 0).rgb;
        o_color = float4(L + L_sun, 1.0);
        return;
    }

    discard; // disable aerial perspective for now

    // Move to top atmosphere as the starting point for ray marching.
    // This is critical to be after the above to not disrupt above atmosphere tests and voxel selection.
    if (!move_to_top_atmosphere(atmosphere, world_pos, world_dir))
    {
        // Ray is not intersecting the atmosphere
        o_color = float4(L_sun, 1);
        return;
    }

    float max_distance = 0.0;
    {
        clip_space.z = max(depth, 0.000001);
        float4 pixel_world_pos = global.camera_inv_view_proj * float4(clip_space, 1.0);
        pixel_world_pos.xyz /= pixel_world_pos.w;
        // reproject world_pos to regular space
        max_distance = distance(world_pos - float3(0.0, atmosphere.bottom_radius, 0.0), pixel_world_pos.xyz);
    }

    float3 L;
    float3 T;
    integrate_luminance(atmosphere, L, T, world_pos, world_dir, global.sun_direction, max_distance);

    const float transmittance = dot(T, float3(1.0 / 3.0));
    o_color = float4(L, 1.0 - transmittance);
}
