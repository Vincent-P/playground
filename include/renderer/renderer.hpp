#pragma once
#include <string>
#include "camera.hpp"
#include "gltf.hpp"
#include "renderer/hl_api.hpp"
#include "renderer/vlk_context.hpp"
#include "render_graph.hpp"


#define assert_uniform_size(T) static_assert(sizeof(T) % 16 == 0, "Uniforms must be aligned to a float4!")

/***
 * The renderer is the orchestrator of the Vulkan Context and the HL API.
 * The main functions are StartFrame and EndFrame and it contains
 * raw HL API calls in between to draw things or validate/cook a Render Graph.
 ***/
namespace my_app
{
namespace UI
{
struct Context;
};

struct Model;
struct Event;
class TimerData;

struct VoxelDebug
{
    float3 center{-13.0f, -1.0f, -11.0f};
    float size{0.1f};
    uint res{256};
};

struct PACKED GlobalUniform
{
    float4x4 camera_view;
    float4x4 camera_proj;
    float4x4 camera_inv_proj;
    float4x4 camera_inv_view_proj;
    float4x4 sun_view;
    float4x4 sun_proj;

    float3 camera_pos;
    float pad00;

    uint2 resolution;
    float2 pad01;


    float3 sun_direction;
    float pad10;

    float3 sun_illuminance;
    float pad11;
};

assert_uniform_size(GlobalUniform);

struct Renderer
{
    static Renderer create(const Window &window, Camera &camera, TimerData &timer, UI::Context &ui);
    void destroy();

    void draw();
    void on_resize(int width, int height);
    void wait_idle();

    void reload_shader(std::string_view shader_name);

    // glTF
    void load_model_data();
    void draw_model();
    void destroy_model();

    // ImGui
    void imgui_draw();

    void voxelize_scene();
    void inject_direct_lighting();
    void generate_aniso_voxels();
    void visualize_voxels();
    void composite_hdr();

    /// --- New

    vulkan::API api;
    RenderGraph graph;

    UI::Context *p_ui;
    const Window *p_window;
    Camera *p_camera;
    TimerData *p_timer;

    ImageDescH depth_buffer;
    ImageDescH hdr_buffer;

    /// --- Render Passes

    struct CheckerBoardFloorPass
    {
        vulkan::BufferH index_buffer;
        vulkan::BufferH vertex_buffer;
        vulkan::GraphicsProgramH program;
    } checkerboard_floor;


    struct ImGuiPass
    {
        vulkan::GraphicsProgramH float_program;
        vulkan::GraphicsProgramH uint_program;
        vulkan::ImageH font_atlas;
    } imgui;

    ImageDescH transmittance_lut;
    ImageDescH skyview_lut;
    ImageDescH multiscattering_lut;

    struct ProceduralSkyPass
    {
        vulkan::CircularBufferPosition atmosphere_params_pos;
        vulkan::GraphicsProgramH render_transmittance;
        vulkan::GraphicsProgramH render_skyview;
        vulkan::ComputeProgramH  compute_multiscattering_lut;
        vulkan::GraphicsProgramH sky_raymarch;
    } procedural_sky;

    struct TonemappingPass
    {
        vulkan::GraphicsProgramH program;
        vulkan::CircularBufferPosition params_pos;
    } tonemapping;

    /// --- Old

    vulkan::RenderTargetH depth_rt;
    vulkan::RenderTargetH color_rt;
    vulkan::RenderTargetH swapchain_rt;

    /// --- Rendering
    vulkan::SamplerH default_sampler;
    vulkan::CircularBufferPosition global_uniform_pos;
    Camera sun;

    // Tonemap
    vulkan::GraphicsProgramH hdr_compositing;


    // Unreal Engine sky
    struct SkyPass
    {
        vulkan::RenderTargetH transmittance_lut_rt;
        vulkan::GraphicsProgramH render_transmittance;

        vulkan::RenderTargetH skyview_lut_rt;
        vulkan::GraphicsProgramH render_skyview;

        vulkan::ImageH multiscattering_lut;
        vulkan::ComputeProgramH compute_multiscattering_lut;

        vulkan::GraphicsProgramH sky_raymarch;
    } sky;

    // glTF
    Model model;

    // Shadow Map
    vulkan::GraphicsProgramH model_prepass;
    vulkan::RenderTargetH screenspace_lod_map_rt;
    vulkan::RenderTargetH shadow_map_rt;
    std::vector<vulkan::ImageH> min_lod_map_per_frame; // we read back it every frame so n-plicate it to avoid gpu stall
    vulkan::ComputeProgramH fill_min_lod_map;

    // Voxelization
    VoxelDebug voxel_options{};
    vulkan::GraphicsProgramH voxelization;
    vulkan::GraphicsProgramH visualization;
    vulkan::ComputeProgramH inject_radiance;
    vulkan::ComputeProgramH generate_aniso_base;
    vulkan::ComputeProgramH generate_aniso_mipmap;

    vulkan::SamplerH trilinear_sampler;
    vulkan::SamplerH nearest_sampler;
    vulkan::ImageH voxels_albedo;
    vulkan::ImageH voxels_normal;
    vulkan::ImageH voxels_radiance;
    std::vector<vulkan::ImageH> voxels_directional_volumes;
};

} // namespace my_app
