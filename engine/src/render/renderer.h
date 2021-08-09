#pragma once

#include <exo/handle.h>

#include "render/base_renderer.h"
#include "render/mesh.h"
#include "render/ring_buffer.h"
#include "render/streamer.h"
#include "render/bvh.h"
#include "render/unified_buffer_storage.h"

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
    float resolution_scale = 1.0f;
    uint2 render_resolution;
    bool resolution_dirty;
    bool enable_taa = true;
    bool clear_history = true;
    bool enable_path_tracing = false;
};

struct ImGuiPass
{
    Handle<gfx::GraphicsProgram> program;
    Handle<gfx::Image>  font_atlas;
};

struct PACKED GlobalUniform
{
    float4x4 camera_view;
    float4x4 camera_projection;
    float4x4 camera_view_inverse;
    float4x4 camera_projection_inverse;
    float4x4 camera_previous_view;
    float4x4 camera_previous_projection;

    float2 render_resolution;
    float2 jitter_offset;

    float delta_t;
    u32 frame_count;
    u32 first_accumulation_frame;
    u32 meshes_data_descriptor;

    u32 instances_data_descriptor;
    u32 instances_offset;
    u32 submesh_instances_data_descriptor;
    u32 submesh_instances_offset;

    u32 tlas_descriptor;
    u32 submesh_instances_count;
    u32 index_buffer_descriptor;
    u32 pad10;
};

struct PACKED PushConstants
{
    u32 draw_id        = u32_invalid;
    u32 gui_texture_id = u32_invalid;
};

// A 3D model
struct RenderMesh
{
    Handle<gfx::Buffer> positions;
    Handle<gfx::Buffer> bvh;
    Handle<gfx::Buffer> submeshes;
    BVHNode bvh_root = {};

    Vec<u32> instances;
    u32 first_instance = 0;
};

// One drawcall, one material instance
struct SubMeshInstance
{
    u32 i_mesh;
    u32 i_submesh;
    u32 i_instance;
    u32 i_draw;
};

// Descriptors for a mesh
struct PACKED RenderMeshGPU
{
    u32 positions_descriptor;
    u32 first_index;
    u32 bvh_descriptor;
    u32 submeshes_descriptor;
};

// One object in the world
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
    Handle<gfx::Image> history_buffers[2];
    Handle<gfx::Framebuffer> hdr_depth_fb;
    Handle<gfx::Framebuffer> ldr_fb;
    Handle<gfx::Framebuffer> ldr_depth_fb;

    // Geometry
    Vec<RenderMesh> render_meshes;
    Vec<RenderInstance> render_instances;
    Handle<gfx::Buffer> render_meshes_buffer;
    Handle<gfx::Buffer> tlas_buffer;
    UnifiedBufferStorage index_buffer;


    // Draw data
    Vec<SubMeshInstance> submesh_instances_to_draw;
    Vec<u32> instances_to_draw;
    RingBuffer instances_data;
    RingBuffer submesh_instances_data;
    Handle<gfx::Buffer> draw_arguments;

    // global uniform data
    u32 instances_offset = 0;
    u32 submesh_instances_offset = 0;
    u32 first_accumulation_frame = 0;

    ImGuiPass imgui_pass;

    Handle<gfx::GraphicsProgram> opaque_program;
    Handle<gfx::ComputeProgram> tonemap_program;
    Handle<gfx::ComputeProgram> taa_program;
    float2 halton_sequence[16];
    Handle<gfx::ComputeProgram> path_tracer_program;

    u32 draw_count;
    Handle<gfx::ComputeProgram> gen_draw_calls_program;

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
