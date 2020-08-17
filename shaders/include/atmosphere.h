#ifndef ATMOSPHERE_H
#define ATMOSPHERE_H

// An atmosphere layer of width 'width', and whose density is defined as
//   'exp_term' * exp('exp_scale' * h) + 'linear_term' * h + 'constant_term',
// clamped to [0,1], and where h is the altitude.
struct DensityProfileLayer
{
    float exp_term;
    float exp_scale;
    float linear_term;
    float constant_term;
};

// An atmosphere density profile made of several layers on top of each other
// (from bottom to top). The width of the last layer is ignored, i.e. it always
// extend to the top atmosphere boundary. The profile values vary between 0
// (null density) to 1 (maximum density).
struct DensityProfile
{
    DensityProfileLayer layers[2];
    float3 pad10;
    float width;
};

struct AtmosphereParameters
{
    // The solar irradiance at the top of the atmosphere.
    float3 solar_irradiance;
    // The sun's angular radius. Warning: the implementation uses approximations
    // that are valid only if this angle is smaller than 0.1 radians.
    float sun_angular_radius;
    // The density profile of air molecules, i.e. a function from altitude to
    // dimensionless values between 0 (null density) and 1 (maximum density).
    DensityProfile rayleigh_density;
    // The density profile of aerosols, i.e. a function from altitude to
    // dimensionless values between 0 (null density) and 1 (maximum density).
    DensityProfile mie_density;
    // The density profile of air molecules that absorb light (e.g. ozone), i.e.
    // a function from altitude to dimensionless values between 0 (null density)
    // and 1 (maximum density).
    DensityProfile absorption_density;
    // The scattering coefficient of air molecules at the altitude where their
    // density is maximum (usually the bottom of the atmosphere), as a function of
    // wavelength. The scattering coefficient at altitude h is equal to
    // 'rayleigh_scattering' times 'rayleigh_density' at this altitude.
    float3 rayleigh_scattering;
    // The distance between the planet center and the bottom of the atmosphere.
    float bottom_radius;
    // The scattering coefficient of aerosols at the altitude where their density
    // is maximum (usually the bottom of the atmosphere), as a function of
    // wavelength. The scattering coefficient at altitude h is equal to
    // 'mie_scattering' times 'mie_density' at this altitude.
    float3 mie_scattering;
    // The distance between the planet center and the top of the atmosphere.
    float top_radius;
    // The extinction coefficient of aerosols at the altitude where their density
    // is maximum (usually the bottom of the atmosphere), as a function of
    // wavelength. The extinction coefficient at altitude h is equal to
    // 'mie_extinction' times 'mie_density' at this altitude.
    float3 mie_extinction;
    // The asymetry parameter for the Cornette-Shanks phase function for the
    // aerosols.
    float mie_phase_function_g;
    // The extinction coefficient of molecules that absorb light (e.g. ozone) at
    // the altitude where their density is maximum, as a function of wavelength.
    // The extinction coefficient at altitude h is equal to
    // 'absorption_extinction' times 'absorption_density' at this altitude.
    float3 absorption_extinction;
    float pad0;
    // The average albedo of the ground.
    float3 ground_albedo;
    // The cosine of the maximum Sun zenith angle for which atmospheric scattering
    // must be precomputed (for maximum precision, use the smallest Sun zenith
    // angle yielding negligible sky light radiance values. For instance, for the
    // Earth case, 102 degrees is a good choice - yielding mu_s_min = -0.2).
    float mu_s_min;
};

#ifndef __cplusplus


/// --- Utils

float safe_sqrt(float a) {
  return sqrt(max(a, 0.0));
}

float clamp_cos(float theta) {
  return clamp(theta, float(-1.0), float(1.0));
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

struct MediumRGB
{
    float3 rayleigh_scattering;
    float3 mie_scattering;
    float3 scattering;
    float3 extinction;
};

MediumRGB sample_medium(AtmosphereParameters atmosphere, float r)
{
    MediumRGB medium;
    float rayleigh_density = get_profile_density(atmosphere.rayleigh_density, r - atmosphere.bottom_radius);
    float mie_density = get_profile_density(atmosphere.mie_density, r - atmosphere.bottom_radius);
    float absorption_density = get_profile_density(atmosphere.absorption_density, r - atmosphere.bottom_radius);


    medium.mie_scattering = mie_density * atmosphere.mie_scattering;
    float3 mie_absorption = mie_density * (atmosphere.mie_extinction - atmosphere.mie_scattering);
    float3 mie_extinction = medium.mie_scattering + mie_absorption;

    medium.rayleigh_scattering = rayleigh_density * atmosphere.rayleigh_scattering;
    float3 rayleigh_absorption = float3(0.0);
    float3 rayleigh_extinction = medium.rayleigh_scattering + rayleigh_absorption;

    float3 absorption_scattering = float3(0.0);
    float3 absorption_absorption = absorption_density * atmosphere.absorption_extinction;
    float3 absorption_extinction = absorption_scattering + absorption_absorption;

    medium.scattering = medium.mie_scattering + medium.rayleigh_scattering + absorption_scattering;
    float3 absorption = mie_absorption + rayleigh_absorption + absorption_absorption;
    medium.extinction = mie_extinction + rayleigh_extinction + absorption_extinction;

    return medium;
}

/// --- Geometric functions

float distance_to_top_atmosphere(AtmosphereParameters atmosphere, float r, float mu)
{
    // discriminant is actually divided by four
    float discriminant = r * r * (mu * mu - 1.0) + atmosphere.top_radius * atmosphere.top_radius;
    // because discriminant is divided by four it cancels the 1/2a
    float root = -r * mu + safe_sqrt(discriminant);
    return root;
}

float distance_to_bottom_atmosphere(AtmosphereParameters atmosphere, float r, float mu)
{
    // discriminant is actually divided by four
    float discriminant = r * r * (mu * mu - 1.0) + atmosphere.bottom_radius * atmosphere.bottom_radius;
    // because discriminant is divided by four it cancels the 1/2a
    float root = -r * mu - safe_sqrt(discriminant);
    return root;
}

bool intersects_ground(AtmosphereParameters atmosphere, float r, float mu)
{
    return mu < 0.0
        && r * r * (mu * mu - 1.0) + atmosphere.bottom_radius * atmosphere.bottom_radius >= 0.0;
}

bool intersects_top_atmosphere(AtmosphereParameters atmosphere, float r, float mu)
{
    return mu < 0.0
        && r * r * (mu * mu - 1.0) + atmosphere.top_radius * atmosphere.top_radius >= 0.0;
}

float distance_to_nearest_atmosphere(AtmosphereParameters atmosphere, float r, float mu, bool intersects_ground)
{
    if (intersects_ground)
    {
        return distance_to_bottom_atmosphere(atmosphere, r, mu);
    }

    return distance_to_top_atmosphere(atmosphere, r, mu);
}

/// --- UV encoding functions

/*
  Storing a function from [0, 1] in a texture of size n will sample the function at 0.5/n, 1.5/n, ... (n - 0.5)/n
because textures samples are at the center of texels . To avoid this f(0) should be a the center of texel 0 and f(1) at
the center of texel n-1.
  These functions map values in [0, 1] to uvs in [0.5/n, 1 - 0.5/n]
 */
float uv_to_unit(float u, float resolution) { return (u - 0.5 / resolution) * (resolution / (resolution - 1.0)); }
float unit_to_uv(float u, float resolution) { return (u + 0.5 / resolution) * (resolution / (resolution + 1.0)); }
float2 uv_to_unit(float2 uv, float2 resolution) { return float2(uv_to_unit(uv.x, resolution.x), uv_to_unit(uv.y, resolution.y)); }
float2 unit_to_uv(float2 units, float2 resolution) { return float2(unit_to_uv(units.x, resolution.x), unit_to_uv(units.y, resolution.y)); }

float2 r_mu_to_uv(AtmosphereParameters atmosphere, float r, float mu)
{
    // distance from P to horizon
    float rho = safe_sqrt(r * r - atmosphere.bottom_radius * atmosphere.bottom_radius);
    // distance from horizon to top atmosphere
    float H = safe_sqrt(atmosphere.top_radius * atmosphere.top_radius - atmosphere.bottom_radius * atmosphere.bottom_radius);

    float d = distance_to_top_atmosphere(atmosphere, r, mu);
    float d_min = atmosphere.top_radius - r;
    float d_max = rho + H;
    float x_mu = (d - d_min) / (d_max - d_min);
    float x_r  = (rho / H);
    return float2(x_mu, x_r);
}

void uv_to_r_mu(AtmosphereParameters atmosphere, float2 uv, out float r, out float mu)
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

void uv_to_mu_coslightview(AtmosphereParameters atmosphere, float2 uv, float r, out float mu, out float cos_lightview)
{
    const float2 LUT_SIZE = float2(192.0, 108.0);
    uv = uv_to_unit(uv, LUT_SIZE);

    float rho = safe_sqrt(r * r - atmosphere.bottom_radius * atmosphere.bottom_radius);
    // angle between the ground to the horizon
    float cos_beta = rho / r;
    float ground_horizon = acos(cos_beta);
    float zenith_horizon = PI - ground_horizon;

    if (uv.y < 0.5)
    {
        float coord = 2.0 * uv.y;
        coord = 1.0 - coord;
        coord *= coord;
        coord = 1.0 - coord;
        mu = cos(zenith_horizon * coord);
    }
    else
    {
        float coord = uv.y * 2.0 - 1.0;
        coord *= coord;
        mu = cos(zenith_horizon + ground_horizon * coord);
    }

    float coord = uv.x;
    coord *= coord;
    cos_lightview = - (coord * 2.0 - 1.0);
}

float2 mu_coslightview_to_uv(AtmosphereParameters atmosphere, bool intersects_ground, float r, float mu, float cos_lightview)
{
    float rho = safe_sqrt(r * r - atmosphere.bottom_radius * atmosphere.bottom_radius);
    // angle between the ground to the horizon
    float cos_beta = rho / r;
    float ground_horizon = acos(cos_beta);
    float zenith_horizon = PI - ground_horizon;

    float2 uv;

    if (intersects_ground)
    {
        float coord = acos(mu) / zenith_horizon;
        coord = 1.0 - coord;
        coord = sqrt(coord);
        coord = 1.0 - coord;
        uv.y = coord * 0.5f;
    }
    else
    {
        float coord = (acos(mu) - zenith_horizon) / ground_horizon;
        coord = sqrt(coord);
        uv.y = coord * 0.5 + 0.5;
    }

    float coord = -cos_lightview * 0.5 + 0.5;
    coord = sqrt(coord);
    uv.x = coord;

    const float2 LUT_SIZE = float2(192.0, 108.0);
    uv = unit_to_uv(uv, LUT_SIZE);
    return uv;
}

/// --- LUT fonctions

float3 get_transmittance_to_top_atmosphere(AtmosphereParameters atmosphere, sampler2D transmittance_lut, float r, float mu)
{
    float2 uv = r_mu_to_uv(atmosphere, r, mu);
    return texture(transmittance_lut, uv).rgb;
}

float3 get_transmittance(AtmosphereParameters atmosphere, sampler2D transmittance_lut, float r, float mu, float d, bool intersects_ground)
{
    float rd = safe_sqrt(d * d + r * r + 2 * r * d * mu);
    float mud = clamp_cos((r * mu * d) / rd);
    float3 transmittance;
    if (intersects_ground)
    {
        transmittance = get_transmittance_to_top_atmosphere(atmosphere, transmittance_lut, rd, -mud) / get_transmittance_to_top_atmosphere(atmosphere, transmittance_lut, r, -mu);
    }
    else
    {
        transmittance = get_transmittance_to_top_atmosphere(atmosphere, transmittance_lut, r, mu) / get_transmittance_to_top_atmosphere(atmosphere, transmittance_lut, rd, mud);
    }
    return min(transmittance, float3(1.0));
}

float3 get_transmittance_to_sun(AtmosphereParameters atmosphere, sampler2D transmittance_lut, float r, float mu_s)
{
    float sin_theta_h = atmosphere.bottom_radius / r;
    float cos_theta_h = -safe_sqrt(1.0 - sin_theta_h * sin_theta_h);
    float visible_sun_disc = smoothstep(-sin_theta_h * atmosphere.sun_angular_radius, sin_theta_h * atmosphere.sun_angular_radius, mu_s - cos_theta_h);
    return get_transmittance_to_top_atmosphere(atmosphere, transmittance_lut, r, mu_s) * visible_sun_disc;
}

/// --- Phase functions

const float uniform_phase = 1.0 / (4.0 * PI);

float rayleigh_phase_function(float cos_theta)
{
    float factor = 3.0 / (16.0 * PI);
    return factor * (1.0 + cos_theta * cos_theta);
}

float cornette_shanks_phase_function(float g, float cos_theta)
{
    float k = 3.0 / (8.0 * PI) * (1.0 - g * g) / (2.0 + g * g);
    return k * (1.0 + cos_theta * cos_theta) / pow(1.0 + g * g - 2.0 * g * -cos_theta, 1.5);
}

/// ---


bool move_to_top_atmosphere(AtmosphereParameters atmosphere, inout float3 world_pos, float3 world_dir)
{
    float r = length(world_pos);
    if (r > atmosphere.top_radius)
    {
        float3 up = world_pos / r;
        float  mu = dot(up, world_dir);
        if (intersects_top_atmosphere(atmosphere, r, mu))
        {
            float d = distance_to_top_atmosphere(atmosphere, r, mu);
            world_pos = world_pos + world_dir * d;
        }
        else
        {
            return false; //failed
        }
    }
    return true;
}


#endif

#endif
