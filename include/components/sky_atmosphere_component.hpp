#pragma once
#include "../shaders/include/atmosphere.h"
#include "base/types.hpp"

#include <imgui/imgui.h>
#include <cmath>

namespace my_app
{

struct SkyAtmosphereComponent
{
    // The asymetry parameter for the Cornette-Shanks phase function for the
    // aerosols.
    float mie_phase_function_g = 0.8f;

    float3 mie_scattering_color = {0.577350259f, 0.577350259f, 0.577350259f};
    float mie_scattering_scale = 6.92127514e-06f;

    float3 mie_absorption_color = {0.577350259f, 0.577350259f, 0.577350259f};
    float mie_absorption_scale = 7.69030294e-07f;

    float3 rayleigh_scattering_color = {0.160114273f, 0.374151886f, 0.913440645f};
    float rayleigh_scattering_scale =  3.62366191e-05f;

    float3 absorption_color = {0.326312512, 0.944298208, 0.0426716395};
    float absorption_scale = 1.99195551e-06f;

    float planet_radius     = 6360000.0f;
    float atmosphere_height = 100000.0f;

    float mie_scale_height = 1200.0;
    float rayleigh_scale_height = 8000.0;

    float3 ground_albedo = {0.0f, 0.0f, 0.0f};

    static const char *type_name() { return "SkyAtmosphereComponent"; }

    inline void display_ui()
    {
        // ImGui::SliderFloat("Mie phase", &mie_phase_function_g, 0.0f, 0.999f, "%.3f", 1.0f / 3.0f);
        ImGui::ColorEdit3("MieScattCoeff", mie_scattering_color.data());
        // ImGui::SliderFloat("MieScattScale", &mie_scattering_scale, 0.00001f, 0.1f, "%.5f", 3.0f);
        ImGui::ColorEdit3("MieAbsorCoeff", mie_absorption_color.data());
        // ImGui::SliderFloat("MieAbsorScale", &mie_absorption_scale, 0.00001f, 10.0f, "%.5f", 3.0f);
        ImGui::ColorEdit3("RayScattCoeff", rayleigh_scattering_color.data());
        // ImGui::SliderFloat("RayScattScale", &rayleigh_scattering_scale, 0.00001f, 10.0f, "%.5f", 3.0f);
        ImGui::ColorEdit3("AbsorptiCoeff", absorption_color.data());
        // ImGui::SliderFloat("AbsorptiScale", &absorption_scale, 0.00001f, 10.0f, "%.5f", 3.0f);
        ImGui::SliderFloat("Planet radius", &planet_radius, 100.0f, 8000.0f);
        ImGui::SliderFloat("Atmos height", &atmosphere_height, 10.0f, 150.0f);
        ImGui::SliderFloat("MieScaleHeight", &mie_scale_height, 0.5f, 20.0f);
        ImGui::SliderFloat("RayScaleHeight", &rayleigh_scale_height, 0.5f, 20.0f);
        ImGui::ColorEdit3("Ground albedo", ground_albedo.data());
    }
};

inline SkyAtmosphereComponent component_from_parameters(const AtmosphereParameters &params)
{
    SkyAtmosphereComponent component;

    auto mie_absorption = params.mie_extinction - params.mie_scattering;

    component.mie_phase_function_g = params.mie_phase_function_g;

#define COLOR_FROM_SCALE(raw, norm) (norm == 0.0f ? float3(0.0f) : (1.0f/norm) * raw)

    component.mie_scattering_scale = params.mie_scattering.norm();
    component.mie_scattering_color = COLOR_FROM_SCALE(params.mie_scattering, component.mie_scattering_scale);

    component.mie_absorption_scale = mie_absorption.norm();
    component.mie_absorption_color = COLOR_FROM_SCALE(mie_absorption, component.mie_absorption_scale);

    component.rayleigh_scattering_scale = params.rayleigh_scattering.norm();
    component.rayleigh_scattering_color = COLOR_FROM_SCALE(params.rayleigh_scattering, component.rayleigh_scattering_scale);

    component.absorption_scale = params.absorption_extinction.norm();
    component.absorption_color = COLOR_FROM_SCALE(params.absorption_extinction, component.absorption_scale);

#undef COLOR_FROM_SCALE

    component.planet_radius = params.bottom_radius;
    component.atmosphere_height = params.top_radius - params.bottom_radius;

    component.mie_scale_height = -1.0f / params.mie_density.layers[1].exp_scale;
    component.rayleigh_scale_height = -1.0f / params.rayleigh_density.layers[1].exp_scale;

    component.ground_albedo = params.ground_albedo;

    return component;
}

inline AtmosphereParameters parameters_from_component(const SkyAtmosphereComponent &component)
{
    AtmosphereParameters params;

    // Using a normalise sun illuminance. This is to make sure the LUTs acts as a
    // transfert factor to apply the runtime computed sun irradiance over.
    params.solar_irradiance = {1.0f, 1.0f, 1.0f};

    params.sun_angular_radius = 0.004675f;

    // rayleigh
    params.rayleigh_density.width     = 0.0f;
    params.rayleigh_density.layers[0] = {
        .exp_term  = 1.0f,
        .exp_scale = -1.0f / component.rayleigh_scale_height,
    };
    params.rayleigh_scattering = component.rayleigh_scattering_scale * component.rayleigh_scattering_color;

    // mie
    params.mie_density.width     = 0.0f;
    params.mie_density.layers[0] = {
        .exp_term  = 1.0f,
        .exp_scale = -1.0f / component.mie_scale_height,
    };

    params.mie_scattering = component.mie_scattering_scale * component.mie_scattering_color;
    params.mie_extinction = params.mie_scattering + component.mie_absorption_scale * component.mie_absorption_color;
    params.mie_phase_function_g = component.mie_phase_function_g;

    // ozone
    params.absorption_density.width     = 25000.0f;
    params.absorption_density.layers[0] = DensityProfileLayer{
        .linear_term   = 1.0f / 15000.0f,
        .constant_term = -2.0f / 3.0f,
    };

    params.absorption_density.layers[1] = DensityProfileLayer{
        .linear_term   = -1.0f / 15000.0f,
        .constant_term = 8.0f / 3.0f,
    };

    params.absorption_extinction = component.absorption_scale * component.absorption_color;


    params.top_radius = component.planet_radius + component.atmosphere_height;
    params.bottom_radius = component.planet_radius;

    params.ground_albedo = component.ground_albedo;

    const double max_sun_zenith_angle = PI * 120.0 / 180.0; // (use_half_precision_ ? 102.0 : 120.0) / 180.0 * kPi;
    params.mu_s_min                   = (float)std::cos(max_sun_zenith_angle);

    return params;
}
}; // namespace my_app
