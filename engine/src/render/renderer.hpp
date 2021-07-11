#pragma once

#include "base/handle.hpp"

#include "render/vulkan/commands.hpp"
#include "render/vulkan/context.hpp"
#include "render/vulkan/device.hpp"
#include "render/vulkan/resources.hpp"
#include "render/vulkan/surface.hpp"

#include "render/mesh.hpp"
#include "render/ring_buffer.hpp"
#include "render/streamer.hpp"

namespace gfx = vulkan;

inline constexpr uint FRAME_QUEUE_LENGTH  = 2;
inline constexpr u32 TIMESTAMPS_PER_FRAME = 16;

namespace gltf {struct Model;}
namespace UI {struct Context;}
struct Mesh;
struct Material;
class Scene;
class AssetManager;

struct RenderTargets
{
    Handle<gfx::RenderPass> clear_renderpass;
    Handle<gfx::RenderPass> load_renderpass;
    Handle<gfx::Framebuffer> framebuffer;
    Handle<gfx::Image> image;
    Handle<gfx::Image> depth;
};

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
    Settings settings;
    AssetManager *asset_manager;

    // Base renderer
    gfx::Context context;
    gfx::Device device;
    gfx::Surface surface;
    uint frame_count;
    std::array<gfx::WorkPool, FRAME_QUEUE_LENGTH> work_pools;
    std::array<gfx::QueryPool, FRAME_QUEUE_LENGTH> timestamp_pools;
    Vec<u64> timestamps;
    gfx::Fence fence;

    RingBuffer dynamic_uniform_buffer;
    RingBuffer dynamic_vertex_buffer;
    RingBuffer dynamic_index_buffer;

    Handle<gfx::Image> empty_sampled_image;
    Handle<gfx::Image> empty_storage_image;

    // User renderer
    Streamer streamer;

    Handle<gfx::Image> depth_buffer;
    RenderTargets hdr_rt;
    RenderTargets ldr_rt;
    RenderTargets swapchain_rt;

    Vec<RenderMesh> render_meshes;
    Vec<RenderInstance> render_instances;
    RingBuffer instances_data;

    ImGuiPass imgui_pass;

    // Geometry pools
    Handle<gfx::GraphicsProgram> opaque_program;
    Handle<gfx::ComputeProgram> tonemap_program;

    /// ---

    static Renderer create(const platform::Window &window, AssetManager *_asset_manager);
    void destroy();

    void display_ui(UI::Context &ui);
    void update(Scene &scene);

    // Ring buffer uniforms
    void *bind_shader_options(gfx::ComputeWork &cmd, Handle<gfx::GraphicsProgram> program, usize options_len);
    void *bind_shader_options(gfx::ComputeWork &cmd, Handle<gfx::ComputeProgram> program, usize options_len);
    template<typename T> T *bind_shader_options(gfx::ComputeWork &cmd, Handle<gfx::GraphicsProgram> program) { return (T*)bind_shader_options(cmd, program, sizeof(T)); }
    template<typename T> T *bind_shader_options(gfx::ComputeWork &cmd, Handle<gfx::ComputeProgram> program) { return (T*)bind_shader_options(cmd, program, sizeof(T)); }

    void reload_shader(std::string_view shader_name);
    void on_resize();
    bool start_frame();
    bool end_frame(gfx::ComputeWork &cmd);
};
