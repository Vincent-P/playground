#pragma once
#include <string>
#include "camera.hpp"
#include "gltf.hpp"
#include "renderer/hl_api.hpp"
#include "renderer/vlk_context.hpp"
#include "render_graph.hpp"
#include <memory>


#define assert_uniform_size(T) static_assert(sizeof(T) % 16 == 0, "Uniforms must be aligned to a float4!")

/***
 * The renderer is the orchestrator of the Vulkan Context and the HL API.
 * The main functions are StartFrame and EndFrame and it contains
 * raw HL API calls in between to draw things or validate/cook a Render Graph.
 ***/
namespace window {struct Window;}
namespace my_app
{
namespace UI
{
struct Context;
};

struct Model;
struct Event;
class TimerData;

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
    float camera_near;
    float camera_far;


    float3 sun_direction;
    float pad10;

    float3 sun_illuminance;
    float ambient;
};

assert_uniform_size(GlobalUniform);

struct VoxelOptions
{
    float3 center = {-16.0f, -10.0f, -15.0f};
    float size    = 0.13f;
    uint res      = 256;
};

struct VCTDebug
{
    uint display = 1; // 0: glTF 1: voxels 2: custom
    uint display_selected = 1; // Voxels (0: albedo 1: normal 2: radiance) glTF (0: nothing 1: base color 2: normal 3: ao 4: indirect lighting)
    int  voxel_selected_mip = 0;
    uint padding00 = 1;

    // cone tracing
    float trace_dist = 6.0f;
    float occlusion_lambda = 1.0f;
    float sampling_factor = 1.0f;
    float start = 1.0f;

    // voxel direct lighting
    float4 point_position  = {1.5f, 2.5f, 0.0f, 0.0f};
    float point_scale      = 1.0f;
    float trace_shadow_hit = 1.0f;
    float max_dist         = 256.f;
    float first_step       = 3.0f;
};

struct Settings
{
    float resolution_scale = 1.0f;
    uint shadow_cascades_count = 4;
};

struct GltfPushConstant
{
    // uniform
    u32 node_idx;
    u32 vertex_offset;

    // textures
    u32 base_color_idx;
    u32 normal_map_idx;
    u32 metallic_roughness_idx;

    // material
    float metallic_factor;
    float roughness_factor;
    float pad00;
    float4 base_color_factor;
};

assert_uniform_size(GltfPushConstant);
static_assert(sizeof(GltfPushConstant) <= 128);

struct Renderer
{
    static void create(Renderer &r, const window::Window &window, Camera &camera, TimerData &timer, UI::Context &ui);
    void destroy();

    void display_ui(UI::Context &ui);
    void draw();
    void on_resize(int width, int height);
    void wait_idle();
    void reload_shader(std::string_view shader_name);

    UI::Context *p_ui;
    Camera *p_camera;
    TimerData *p_timer;

    vulkan::API api;
    RenderGraph graph;

    Camera sun;
    Settings settings;
    std::shared_ptr<Model> model;

    ImageDescH depth_buffer;
    ImageDescH hdr_buffer;
    ImageDescH ldr_buffer;

    vulkan::CircularBufferPosition global_uniform_pos;
    vulkan::SamplerH nearest_sampler;
    vulkan::SamplerH trilinear_sampler;

    /// --- Render Passes

    struct CheckerBoardFloorPass
    {
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

    struct GltfPass
    {
        vulkan::BufferH vertex_buffer;
        vulkan::BufferH index_buffer;
        vulkan::GraphicsProgramH shading;
        vulkan::GraphicsProgramH prepass;
        vulkan::GraphicsProgramH shadow_cascade_program;
        std::vector<vulkan::ImageH> images;
        std::vector<vulkan::SamplerH> samplers;
        std::shared_ptr<Model> model;
    } gltf;

    ImageDescH voxels_albedo;
    ImageDescH voxels_normal;
    ImageDescH voxels_radiance;

    std::array<ImageDescH, 6> voxels_directional_volumes;
    std::vector<ImageDescH> shadow_cascades;
    std::vector<float> depth_slices;
    std::vector<float4x4> cascades_view;
    std::vector<float4x4> cascades_proj;
    vulkan::CircularBufferPosition matrices_pos;
    vulkan::CircularBufferPosition depth_slices_pos;

    struct VoxelPass
    {
        vulkan::GraphicsProgramH voxelization;
        vulkan::GraphicsProgramH debug_visualization;
        vulkan::ComputeProgramH clear_voxels;
        vulkan::ComputeProgramH inject_radiance;
        vulkan::ComputeProgramH generate_aniso_base;
        vulkan::ComputeProgramH generate_aniso_mipmap;
        vulkan::CircularBufferPosition voxel_options_pos;
        vulkan::CircularBufferPosition projection_cameras;
        vulkan::CircularBufferPosition vct_debug_pos;
    } voxels;

    VoxelOptions voxel_options;
    VCTDebug vct_debug;

    // todo clean this pls
    float ambient = 0.0f;
    float3 sun_illuminance = 100.0f;

};

} // namespace my_app
