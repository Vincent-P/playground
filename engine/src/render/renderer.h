#pragma once

#include <exo/handle.h>

#include "render/base_renderer.h"
#include "render/mesh.h"
#include "render/ring_buffer.h"
#include "render/streamer.h"
#include "render/bvh.h"

#include "render/vulkan/commands.h"
#include "render/vulkan/context.h"
#include "render/vulkan/device.h"
#include "render/vulkan/resources.h"
#include "render/vulkan/surface.h"

#include <chrono>

namespace gfx = vulkan;

namespace gltf {struct Model;}
namespace UI {struct Context;}
struct Mesh;
struct Material;
class Scene;
class AssetManager;

struct Settings
{
    float resolution_scale = 0.5f;
    uint2 render_resolution;
    bool resolution_dirty;
    bool enable_taa = true;
    bool enable_path_tracing = false;
};

struct ImGuiPass
{
    Handle<gfx::GraphicsProgram> program;
    Handle<gfx::Image>  font_atlas;
};

struct PACKED TonemapOptions
{
    uint sampled_hdr_buffer;
    uint storage_output_frame;
};

struct PACKED GlobalUniform
{
    float4x4 camera_view;
    float4x4 camera_projection;
    float4x4 camera_view_inverse;
    float4x4 camera_projection_inverse;
    float4x4 camera_previous_view;
    float4x4 camera_previous_projection;
    float2 resolution;
    float delta_t;
    u32 frame_count;
    float2 jitter_offset;
};

struct PACKED PushConstants
{
    u32 draw_id        = u32_invalid;
    u32 gui_texture_id = u32_invalid;
};

struct RenderMesh
{
    Handle<gfx::Buffer> positions;
    Handle<gfx::Buffer> indices;
    Handle<gfx::Buffer> bvh;
    BVHNode bvh_root = {};
    Vec<SubMesh> submeshes;

    Vec<u32> instances;
    u32 first_instance = 0;
};

struct PACKED RenderMeshGPU
{
    u32 positions_descriptor;
    u32 indices_descriptor;
    u32 bvh_descriptor;
    u32 pad01;
};

struct PACKED RenderInstance
{
    float4x4 object_to_world;
    float4x4 world_to_object;
    u32 i_render_mesh;
    u32 pad00;
    u32 pad01;
    u32 pad10;
};


struct Renderer
{
    BaseRenderer base_renderer;

    AssetManager *asset_manager;
    Settings settings;
    Streamer streamer;

    Handle<gfx::Image> depth_buffer;
    Handle<gfx::Image> hdr_buffer;
    Handle<gfx::Image> ldr_buffer;
    Handle<gfx::Framebuffer> hdr_depth_fb;
    Handle<gfx::Framebuffer> ldr_fb;
    Handle<gfx::Framebuffer> ldr_depth_fb;

    Vec<RenderMesh> render_meshes;
    Vec<RenderInstance> render_instances;
    Handle<gfx::Buffer> render_meshes_buffer;
    RingBuffer instances_data;
    u32 first_instance = 0;

    Vec<u32> meshes_to_draw;
    Vec<u32> instances_to_draw;
    Handle<gfx::Buffer> tlas_buffer;

    ImGuiPass imgui_pass;

    Handle<gfx::GraphicsProgram> opaque_program;
    Handle<gfx::ComputeProgram> tonemap_program;
    Handle<gfx::ComputeProgram> path_tracer_program;

    /// ---

    static Renderer create(const platform::Window &window, AssetManager *_asset_manager);
    void destroy();

    void display_ui(UI::Context &ui);
    void update(Scene &scene);

    void prepare_geometry(Scene &scene);

    // base_renderer
    void reload_shader(std::string_view shader_name);
    void on_resize();
    bool start_frame();
    bool end_frame(gfx::ComputeWork &cmd);
};
