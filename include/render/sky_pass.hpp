#pragma once
#include "render/hl_api.hpp"
#include "render/render_graph.hpp"

namespace my_app
{

struct SkyAtmosphereComponent;
struct ProceduralSkyPass
{
    vulkan::CircularBufferPosition atmosphere_params_pos;
    vulkan::GraphicsProgramH render_transmittance;
    vulkan::GraphicsProgramH render_skyview;
    vulkan::ComputeProgramH compute_multiscattering_lut;
    vulkan::GraphicsProgramH sky_raymarch;

    ImageDescH transmittance_lut;
    ImageDescH skyview_lut;
    ImageDescH multiscattering_lut;
};

ProceduralSkyPass create_procedural_sky_pass(vulkan::API &api);
void add_procedural_sky_pass(RenderGraph &graph, ProceduralSkyPass &pass_data, const SkyAtmosphereComponent &sky_atmosphere, ImageDescH depth_buffer, ImageDescH output);

} // namespace my_app
