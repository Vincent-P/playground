#pragma once

#include "base/handle.hpp"

#include "render/base_renderer.hpp"
#include "render/mesh.hpp"
#include "render/ring_buffer.hpp"
#include "render/streamer.hpp"

#include "render/vulkan/commands.hpp"
#include "render/vulkan/context.hpp"
#include "render/vulkan/device.hpp"
#include "render/vulkan/resources.hpp"
#include "render/vulkan/surface.hpp"

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
    u32 vertex_count = 0;
    Vec<SubMesh> submeshes;
};

struct PACKED RenderInstance
{
    float4x4 transform;
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
    RenderTargets hdr_rt;
    RenderTargets ldr_rt;

    Vec<RenderMesh> render_meshes;
    Vec<RenderInstance> render_instances;
    RingBuffer instances_data;

    ImGuiPass imgui_pass;

    Handle<gfx::GraphicsProgram> opaque_program;
    Handle<gfx::ComputeProgram> tonemap_program;

    /// ---

    static Renderer create(const platform::Window &window, AssetManager *_asset_manager);
    void destroy();

    void display_ui(UI::Context &ui);
    void update(Scene &scene);

    void reload_shader(std::string_view shader_name);
    void on_resize();
    bool start_frame();
    bool end_frame(gfx::ComputeWork &cmd);
};
