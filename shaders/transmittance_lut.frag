#include "types.h"
#include "globals.h"

#include "atmosphere.h"

layout(location = 0) out vec4 outColor;

layout (set = 1, binding = 0) uniform AtmosphereUniform {
    AtmosphereParameters atmosphere;
};

float safe_sqrt(float a) {
  return sqrt(max(a, 0.0));
}

float clamp_cos(float mu) {
  return clamp(mu, float(-1.0), float(1.0));
}

/// --- Struct fonctions

float get_layer_density(DensityProfileLayer layer, float h)
{
    float density = layer.exp_term * exp(layer.exp_scale * h) + layer.linear_term * h + layer.constant_term;
    return clamp(density, 0.0, 1.0);
}

float get_profile_density(DensityProfile profile, float h)
{
    uint i = profile.width == 0.0f || h < profile.width ? 0 : 1;
    return get_layer_density(profile.layers[i], h);
}

/// --- Geometric functions

float distance_to_top_atmosphere(float r, float mu)
{
    // discriminant is actually divided by four
    float discriminant = r * r * (mu * mu - 1.0) + atmosphere.top_radius * atmosphere.top_radius;
    // because discriminant is divided by four it cancels the 1/2a
    float root = -r * mu + safe_sqrt(discriminant);
    return root;
}

float distance_to_bottom_atmosphere(float r, float mu)
{
    // discriminant is actually divided by four
    float discriminant = r * r * (mu * mu - 1.0) + atmosphere.bottom_radius * atmosphere.bottom_radius;
    // because discriminant is divided by four it cancels the 1/2a
    float root = -r * mu - safe_sqrt(discriminant);
    return root;
}

bool intersects_ground(float r, float mu)
{
    return mu < 0.0
        && r * r * (mu * mu - 1.0) + atmosphere.bottom_radius * atmosphere.bottom_radius >= 0.0;
}

/// --- Light functions

float integrate_optical_length_to_top_atmosphere(DensityProfile profile, float r, float mu)
{
    const int SAMPLE_COUNT = 100;

    float dx = distance_to_top_atmosphere(r, mu) / float(SAMPLE_COUNT);
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

float2 r_mu_to_uv(float r, float mu)
{
    // distance from P to horizon
    float rho = safe_sqrt(r * r - atmosphere.bottom_radius * atmosphere.bottom_radius);
    // distance from horizon to top atmosphere
    float H = safe_sqrt(atmosphere.top_radius * atmosphere.top_radius - atmosphere.bottom_radius * atmosphere.bottom_radius);

    float d = distance_to_top_atmosphere(r, mu);
    float d_min = atmosphere.top_radius - r;
    float d_max = rho + H;
    float x_mu = (d - d_min) / (d_max - d_min);
    float x_r  = (rho / H);
    return float2(x_mu, x_r);
}

void uv_to_r_mu(float2 uv, out float r, out float mu)
{
    float x_mu = uv.x;
    float x_r  = uv.y;

    // distance from horizon to top atmosphere
    float H = safe_sqrt(atmosphere.top_radius * atmosphere.top_radius - atmosphere.bottom_radius * atmosphere.bottom_radius);
    // distance from P to horizon
    float rho = x_r * H;

    r = safe_sqrt(rho * rho + atmosphere.bottom_radius * atmosphere.bottom_radius);

    float d_min = atmosphere.top_radius - r;
    float d_max = rho + H;
    float d = d_min + x_mu * (d_max - d_min);
    mu = d == 0.0 ? 1.0 : (H * H  - rho * rho - d * d) / (2.0 * r * d);
    mu = clamp_cos(mu);
}

void main()
{
    const float2 TRANSMITTANCE_LUT_SIZE = float2(256, 64); // TODO: uniform?
    float2 pixel_pos = gl_FragCoord.xy;
    float2 uv = pixel_pos / TRANSMITTANCE_LUT_SIZE;

    float r;
    float mu;
    uv_to_r_mu(uv, r, mu);

    float3 transmittance = integrate_transmittance_to_top_atmosphere(r, mu);
    outColor = float4(transmittance, 1.0f);
}
