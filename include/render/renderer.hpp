#pragma once

#include "base/types.hpp"
#include "ecs.hpp"
#include "render/hl_api.hpp"
#include "render/render_graph.hpp"

#include "render/luminance_pass.hpp"
#include "render/imgui_pass.hpp"
#include "render/tonemap_pass.hpp"
#include "render/sky_pass.hpp"

#include <memory>
#include <string_view>

#define assert_uniform_size(T) \
    static_assert(sizeof(T) % 16 == 0, "Uniforms must be aligned to a float4!"); \
    static_assert(sizeof(T) < 64_KiB, "Uniforms maximum range is 64KiB")

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

struct PACKED GlobalUniform
{
    float4x4 camera_view;
    float4x4 camera_proj;
    float4x4 camera_inv_view;
    float4x4 camera_inv_proj;
    float4x4 camera_inv_view_proj;
    float4x4 camera_previous_view;
    float4x4 camera_previous_proj;
    float4x4 sun_view;
    float4x4 sun_proj;

    float3 camera_pos;
    float  delta_t;

    uint2 resolution;
    float camera_near;
    float camera_far;


    float3 sun_direction;
    float pad00;

    float3 sun_illuminance;
    float ambient;

    float2 jitter_offset;
    float2 previous_jitter_offset;
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
    int show_cascades = 0;

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
    uint2 render_resolution    = {.x = 0, .y = 0};
    float resolution_scale     = 1.0;
    bool resolution_dirty      = false;
    uint shadow_cascades_count = 4;
    float split_factor         = 0.80f;
    bool enable_taa            = true;
    bool show_grid             = true;
};

struct GltfPushConstant
{
    u32 draw_id;
    u32 pad00;
    u32 pad01;
    u32 pad10;
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

    std::array<float2, 16> halton_indices;
    float2 previous_jitter;
    ImageDescH *override_main_pass_output = nullptr;

    ImageDescH depth_buffer;
    ImageDescH hdr_buffer;
    ImageDescH ldr_buffer;

    vulkan::CircularBufferPosition global_uniform_pos;
    vulkan::SamplerH nearest_sampler;
    vulkan::SamplerH trilinear_sampler;
    vulkan::SamplerH nearest_repeat_sampler;
    vulkan::SamplerH trilinear_repeat_sampler;

    vulkan::ImageH random_rotations;
    u32 random_rotation_idx;

    /// --- Render Passes

    struct CheckerBoardFloorPass
    {
        vulkan::GraphicsProgramH program;
    } checkerboard_floor;


    ImGuiPass imgui;

    std::array<ImageDescH, 2> history;
    usize current_history = 0;
    ImageDescH taa_output;

    ProceduralSkyPass procedural_sky;
    TonemappingPass tonemapping;
    LuminancePass luminance;

    struct TemporalPass
    {
        vulkan::ComputeProgramH accumulate;
    } temporal_pass;


    struct GltfPass
    {
        vulkan::BufferH vertex_buffer;
        vulkan::BufferH index_buffer;
        vulkan::BufferH material_buffer;
        vulkan::BufferH transforms_buffer;
        vulkan::BufferH primitives_buffer;
        vulkan::BufferH draws_buffer;

        vulkan::DrawIndirectCommands commands;
        vulkan::BufferH commands_buffer;
        vulkan::BufferH visibility_buffer;
        vulkan::BufferH finalcommands_buffer;
        vulkan::BufferH finaldata_buffer;

        vulkan::ComputeProgramH culling;
        vulkan::ComputeProgramH compaction;
        vulkan::GraphicsProgramH shading_simple;
        vulkan::GraphicsProgramH shading;
        vulkan::GraphicsProgramH prepass;
        vulkan::GraphicsProgramH shadow_cascade_program;
        Vec<vulkan::ImageH> images;
        Vec<vulkan::SamplerH> samplers;
        std::shared_ptr<Model> model;
    } gltf;

    ImageDescH voxels_albedo;
    ImageDescH voxels_normal;
    ImageDescH voxels_radiance;

    std::array<ImageDescH, 6> voxels_directional_volumes;
    Vec<ImageDescH> shadow_cascades;

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

    struct CascadesBoundsPass
    {
        vulkan::ComputeProgramH depth_reduction_0;
        vulkan::ComputeProgramH depth_reduction_1;
        vulkan::ComputeProgramH compute_bounds;
        vulkan::BufferH cascades_slices_buffer;
    } cascades_bounds;
    Vec<ImageDescH> depth_reduction_maps;

    VoxelOptions voxel_options;
    VCTDebug vct_debug;

    // todo clean this pls
    float ambient = 0.0f;
    float3 sun_illuminance = 100.0f;

};

// render passes
Renderer::CheckerBoardFloorPass create_floor_pass(vulkan::API &api);
void add_floor_pass(Renderer &r);
Renderer::GltfPass create_gltf_pass(RenderGraph &graph, vulkan::API &api, std::shared_ptr<Model> &model);
Renderer::VoxelPass create_voxel_pass(vulkan::API & api);
Renderer::CascadesBoundsPass create_cascades_bounds_pass(vulkan::API &api);
void add_cascades_bounds_pass(Renderer &r);


} // namespace my_app
