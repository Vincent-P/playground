#include "types.h"
#include "globals.h"

#include "atmosphere.h"

layout(location = 0) out vec4 outColor;

layout (set = 1, binding = 0) uniform AtmosphereUniform {
    AtmosphereParameters atmosphere;
};

float integrate_optical_length_to_top_atmosphere(DensityProfile profile, float r, float mu)
{
    const int SAMPLE_COUNT = 32;

    float dx = distance_to_top_atmosphere(atmosphere, r, mu) / float(SAMPLE_COUNT);
    float optical_length = 0.0;

    // Integrate using Trapezoidal rule
    for (int i = 1; i <= SAMPLE_COUNT; i++)
    {
        float di = i * dx;
        // distance between planet center and sample point
        float ri = safe_sqrt(di * di + r * r + 2 * r * di * mu);

        float density = get_profile_density(profile, ri - atmosphere.bottom_radius);
        float weight = i == 1 || i == SAMPLE_COUNT ? 0.5 : 1.0;
        optical_length += weight * density;
    }

    optical_length *= dx;

    return optical_length;
}

float3 integrate_transmittance_to_top_atmosphere(float r, float mu)
{
    float3 extinction = float3(0.0f);

    extinction += atmosphere.rayleigh_scattering   * integrate_optical_length_to_top_atmosphere(atmosphere.rayleigh_density,   r, mu);
    extinction += atmosphere.mie_extinction        * integrate_optical_length_to_top_atmosphere(atmosphere.mie_density,        r, mu);
    extinction += atmosphere.absorption_extinction * integrate_optical_length_to_top_atmosphere(atmosphere.absorption_density, r, mu);

    return exp(-extinction);
}

/*
L(c,v)   = T(c,p).Lo(p,v) +∫_‖p−c‖_t=0  Lscat(c,c−tv,v)dt  (1)
T(xa,xb) = e−(∫_xb_x=xa  σt(x)‖dx‖)                        (2)
Lscat(c,x,v) = σs(x) Nlight_∑_i=1  T(c,x) S(x,li) p(v,li)Ei (3)
S(x,li) = Vis(li) T(x,x + t_atmo li)                         (4)
 */

void main()
{
    const float2 TRANSMITTANCE_LUT_SIZE = float2(256, 64); // TODO: uniform?
    float2 pixel_pos = gl_FragCoord.xy;
    float2 uv = pixel_pos / TRANSMITTANCE_LUT_SIZE;

    float r;
    float mu;
    uv_to_r_mu(atmosphere, uv, r, mu);

    float3 transmittance = integrate_transmittance_to_top_atmosphere(r, mu);
    outColor = float4(transmittance, 1.0f);
}
