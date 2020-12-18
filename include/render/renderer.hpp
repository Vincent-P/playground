#pragma once

#include "base/types.hpp"
#include "ecs.hpp"
#include "render/hl_api.hpp"
#include "render_graph.hpp"

#include <memory>
#include <string_view>

#define assert_uniform_size(T) static_assert(sizeof(T) % 16 == 0, "Uniforms must be aligned to a float4!")

/***
 * The renderer is the orchestrator of the Vulkan Context and the HL API.
 * The main functions are StartFrame and EndFrame and it contains
 * raw HL API calls in between to draw things or validate/cook a Render Graph.
 ***/
namespace my_app
{

// fwd
namespace platform {struct Window;}
namespace UI {struct Context;}
struct Model;
struct Event;
class TimerData;
struct CameraComponent;
struct SkyAtmosphereComponent;

struct PACKED GlobalUniform
{
    float4x4 camera_view;
    float4x4 camera_proj;
    float4x4 camera_inv_proj;
    float4x4 camera_inv_view_proj;
    float4x4 sun_view;
    float4x4 sun_proj;

    float3 camera_pos;
    float  delta_t;

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
    uint display = 0; // 0: glTF 1: voxels 2: custom
    uint display_selected = 0; // Voxels (0: albedo 1: normal 2: radiance) glTF (0: nothing 1: base color 2: normal 3: ao 4: indirect lighting)
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

struct TonemapDebug
{
    uint selected = 0;
    float exposure = 1.0f;
};

struct Settings
{
    uint2 render_resolution    = {.x = 0, .y = 0};
    float resolution_scale     = 1.0;
    bool resolution_dirty      = false;
    uint shadow_cascades_count = 4;
    float split_factor         = 0.80f;
    bool show_grid             = true;
};

struct GltfPushConstant
{
    // uniform
    u32 node_idx;
    u32 vertex_offset;

    // textures
    u32 random_rotations_idx;
    u32 base_color_idx;
    u32 normal_map_idx;
    u32 metallic_roughness_idx;

    // material
    float metallic_factor;
    float roughness_factor;
    float4 base_color_factor;
};

assert_uniform_size(GltfPushConstant);
static_assert(sizeof(GltfPushConstant) <= 128);

struct ShadowCascadesAndSlices
{
    float4 slice1;
    float4 slice2;

    struct
    {
        float4x4 view;
        float4x4 proj;
    } matrices[4];
};

static_assert(sizeof(ShadowCascadesAndSlices) == 2 * sizeof(float4) + 4 * 2 * sizeof(float4x4));

// Replace with directional light tagged "sun"
struct Sun
{
    float pitch;
    float yaw;
    float roll;
    float3 front;
};

struct Renderer
{
    static void create(Renderer &r, const platform::Window &window, TimerData &timer);
    void destroy();

    void display_ui(UI::Context &ui);
    void draw(ECS::World &world, ECS::EntityId main_camera);
    void on_resize(int window_width, int window_height);
    void wait_idle() const;
    void reload_shader(std::string_view shader_name);

    TimerData *p_timer;

    vulkan::API api;
    RenderGraph graph;

    Sun sun;
    Settings settings;
    std::shared_ptr<Model> model;

    ImageDescH depth_buffer;
    ImageDescH hdr_buffer;
    ImageDescH ldr_buffer;

    vulkan::CircularBufferPosition global_uniform_pos;
    vulkan::SamplerH nearest_sampler;
    vulkan::SamplerH trilinear_sampler;

    vulkan::ImageH random_rotations;
    u32 random_rotation_idx;

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

    struct LuminancePass
    {
        vulkan::BufferH histogram_buffer;
        vulkan::ComputeProgramH build_histo;
        vulkan::ComputeProgramH average_histo;
    } luminance;
    ImageDescH average_luminance;

    struct CascadesBoundsPass
    {
        vulkan::ComputeProgramH depth_reduction_0;
        vulkan::ComputeProgramH depth_reduction_1;
        vulkan::ComputeProgramH compute_bounds;
        vulkan::BufferH cascades_slices_buffer;
    } cascades_bounds;
    std::vector<ImageDescH> depth_reduction_maps;

    VoxelOptions voxel_options;
    VCTDebug vct_debug;
    TonemapDebug tonemap_debug;

    // todo clean this pls
    float ambient = 0.0f;
    float3 sun_illuminance = 100.0f;

};

// render passes
Renderer::CheckerBoardFloorPass create_floor_pass(vulkan::API &api);
void add_floor_pass(Renderer &r);
Renderer::ImGuiPass create_imgui_pass(vulkan::API &api);
void add_imgui_pass(Renderer &r);
Renderer::ProceduralSkyPass create_procedural_sky_pass(vulkan::API &api);
void add_procedural_sky_pass(Renderer &r, const SkyAtmosphereComponent& sky_atmosphere);
Renderer::TonemappingPass create_tonemapping_pass(vulkan::API &api);
void add_tonemapping_pass(Renderer &r);
Renderer::GltfPass create_gltf_pass(vulkan::API &api, std::shared_ptr<Model> &model);
Renderer::VoxelPass create_voxel_pass(vulkan::API & api);
Renderer::LuminancePass create_luminance_pass(vulkan::API & api);
void add_luminance_pass(Renderer &r);
Renderer::CascadesBoundsPass create_cascades_bounds_pass(vulkan::API &api);
void add_cascades_bounds_pass(Renderer &r);


} // namespace my_app
