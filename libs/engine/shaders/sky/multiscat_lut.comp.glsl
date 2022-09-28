#include "engine/globals.h"
#include "atmosphere.h"

layout (set = 1, binding = 0) uniform AtmosphereUniform {
    AtmosphereParameters atmosphere;
};

layout(set = 1, binding = 1) uniform sampler2D transmittance_lut; // sampler linear clamp
layout(set = 1, binding = 2) uniform writeonly image2D multiscattering_lut;

void preintegrate_multiscattering(AtmosphereParameters atmosphere, out float3 L, out float3 Lf, float3 p, float3 dir, float3 sun_dir)
{
    float3 throughput = float3(1.0);
    L  = float3(0.0);
    Lf = float3(0.0);

    float r    = length(p);
    float3 up = p / r;
    float mu   = dot(up, dir);
    float mu_s = dot(up, sun_dir) / r;
    float v    = dot(dir, sun_dir);

    const int SAMPLE_COUNT = 20;

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

        float3 os = medium.scattering;
        float3 T = sun_transmittance;
        float3 S = float3(1.0); // todo?
        float pu = uniform_phase;
        float3 Ei = float3(1.0);

        // L′(x,v) = T(x,p) Lo(p,v) +∫_{t=0}^{‖p−x‖} σs(x) T(x,x−tv) S(x,ωs) pu EI dt      (6)
        float3 L_integrand = os * T * S * pu * Ei;

        /// --- Integrate scattering

        // Lf(x,v) =∫_{t=0}^{‖p−x‖} σs(x) T(x,x−tv) 1 dt     (8)
        // Integration formula, see slide 28 at http://www.frostbite.com/2015/08/physically-based-unified-volumetric-rendering-in-frostbite/
        float3 Lf_integrand = medium.scattering * 1;
        float3 Lf_int = (Lf_integrand - Lf_integrand * sample_transmittance) / medium.extinction; // integrate along the current step segment
        Lf += throughput * Lf_int;                                                                // accumulate and also take into account the transmittance from previous steps

        // same as above
        float3 L_int = (L_integrand - L_integrand * sample_transmittance) / medium.extinction;
        L += throughput * L_int;

        throughput *= sample_transmittance;
    }


    // Account for bounced light off the earth
    if (tBottom > 0.0)
    {
        float3 intersection = p + tBottom * dir;
        float inter_r = length(intersection);

        float mu_s = dot(intersection, sun_dir) / inter_r;
        float3 sun_transmittance = get_transmittance_to_sun(atmosphere, transmittance_lut, inter_r, mu_s);

        const float3 up = intersection / inter_r;
        const float n_dot_l = clamp(dot(up, sun_dir), 0.0, 1.0);
        L += float3(1.0) * sun_transmittance * throughput * n_dot_l * atmosphere.ground_albedo / PI;
    }
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 64) in;

shared float3 Lf_shared[64];
shared float3 L_shared[64];

void main()
{
    float2 lut_size = imageSize(multiscattering_lut);
    float2 uv = (gl_GlobalInvocationID.xy + float2(0.5)) / lut_size;
    uv = uv_to_unit(uv, lut_size);

    float mu = uv.x * 2.0 - 1.0;
    float3 sun_dir = float3(0.0, mu, sqrt(clamp(1.0 - mu * mu, 0.0, 1.0)));

    float r = atmosphere.bottom_radius + uv.y * (atmosphere.top_radius - atmosphere.bottom_radius);


    float3 p = float3(0.0, r, 0.0);
    float3 dir = float3(0.0, 1.0, 0.0);

    const float sphere_solid_angle = 4.0 * PI;
    const float isotropic_phase = 1.0 / sphere_solid_angle;

    const float SQRT_SAMPLE_COUNT = 8.0;
    float i = 0.5 + float(gl_GlobalInvocationID.z / SQRT_SAMPLE_COUNT);
    float j = 0.5 + float(gl_GlobalInvocationID.z - float((gl_GlobalInvocationID.z / SQRT_SAMPLE_COUNT) * SQRT_SAMPLE_COUNT));
    {
            float rand_a    = i / SQRT_SAMPLE_COUNT;
            float rand_b    = j / SQRT_SAMPLE_COUNT;
            float theta     = 2.0 * PI * rand_a;
            float phi       = PI * rand_b;
            float cos_phi   = cos(phi);
            float sin_phi   = sin(phi);
            float cos_theta = cos(theta);
            float sin_theta = sin(theta);
            dir.x = cos_theta * sin_phi;
            dir.y = cos_phi;
            dir.z = sin_theta * sin_phi;

            float3 L;
            float3 Lf;

            preintegrate_multiscattering(atmosphere, L, Lf, p, dir, sun_dir);

            Lf_shared[gl_GlobalInvocationID.z] = Lf * sphere_solid_angle / (SQRT_SAMPLE_COUNT * SQRT_SAMPLE_COUNT);
            L_shared[gl_GlobalInvocationID.z]  = L  * sphere_solid_angle / (SQRT_SAMPLE_COUNT * SQRT_SAMPLE_COUNT);
    }

    barrier();

    // 64 to 32
    if (gl_GlobalInvocationID.z < 32)
    {
        Lf_shared[gl_GlobalInvocationID.z] += Lf_shared[gl_GlobalInvocationID.z + 32];
        L_shared[gl_GlobalInvocationID.z]  += L_shared[gl_GlobalInvocationID.z + 32];
    }

    barrier();

    // 32 to 16
    if (gl_GlobalInvocationID.z < 16)
    {
        Lf_shared[gl_GlobalInvocationID.z] += Lf_shared[gl_GlobalInvocationID.z + 16];
        L_shared[gl_GlobalInvocationID.z]  += L_shared[gl_GlobalInvocationID.z + 16];
    }

    barrier();

    // 16 to 8 (16 is thread group min hardware size with intel, no sync required from there)
    if (gl_GlobalInvocationID.z < 8)
    {
        Lf_shared[gl_GlobalInvocationID.z] += Lf_shared[gl_GlobalInvocationID.z + 8];
        L_shared[gl_GlobalInvocationID.z]  += L_shared[gl_GlobalInvocationID.z + 8];
    }

    barrier();

    if (gl_GlobalInvocationID.z < 4)
    {
        Lf_shared[gl_GlobalInvocationID.z] += Lf_shared[gl_GlobalInvocationID.z + 4];
        L_shared[gl_GlobalInvocationID.z]  += L_shared[gl_GlobalInvocationID.z + 4];
    }

    barrier();

    if (gl_GlobalInvocationID.z < 2)
    {
        Lf_shared[gl_GlobalInvocationID.z] += Lf_shared[gl_GlobalInvocationID.z + 2];
        L_shared[gl_GlobalInvocationID.z]  += L_shared[gl_GlobalInvocationID.z + 2];
    }

    barrier();

    if (gl_GlobalInvocationID.z < 1)
    {
        Lf_shared[gl_GlobalInvocationID.z] += Lf_shared[gl_GlobalInvocationID.z + 1];
        L_shared[gl_GlobalInvocationID.z]  += L_shared[gl_GlobalInvocationID.z + 1];
    }

    barrier();

    if (gl_GlobalInvocationID.z > 0)
        return;

    // fms =∫_{Ω4π} Lf(xs,−ω) pu dω           (7)
    float3 fms   = Lf_shared[0] * isotropic_phase;

    // L_2nd_order = ∫_{Ω4π} L′(xs,−ω) pu dω (5)
    float3 L_2nd = L_shared[0] * isotropic_phase;

    // fms represents the amount of luminance scattered as if the integral of scattered luminance over the sphere would be 1.
    //  - 1st order of scattering: one can ray-march a straight path as usual over the sphere. That is L_2nd.
    //  - 2nd order of scattering: the inscattered luminance is L_2nd at each of samples of fist order integration. Assuming a uniform phase function that is represented by fms,
    //  - 3nd order of scattering: the inscattered luminance is (L_2nd * fms * fms)
    //  - etc.

    // Fms = 1 + fms + fms^2 + fms^3 + ... = 1 / (1 − fms)        (9)
    const float3 Fms = 1.0f / (1.0 - fms);

    // Ψms = L_2nd_order Fms                 (10)
    float3 psi_ms = L_2nd * Fms;

    imageStore(multiscattering_lut, int2(gl_GlobalInvocationID.xy), float4(psi_ms, 1.0));
}
