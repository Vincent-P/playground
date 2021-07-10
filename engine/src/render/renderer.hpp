#pragma once

#include "base/handle.hpp"

#include "render/vulkan/commands.hpp"
#include "render/vulkan/context.hpp"
#include "render/vulkan/device.hpp"
#include "render/vulkan/resources.hpp"
#include "render/vulkan/surface.hpp"

#include "render/ring_buffer.hpp"
#include "render/bvh.hpp"
#include "render/streamer.hpp"

namespace gfx = vulkan;

inline constexpr uint FRAME_QUEUE_LENGTH = 1;

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
    uint sampled_luminance_output;
    uint storage_output_frame;
    uint selected;
    float exposure;
};

struct TonemapPass
{
    Handle<gfx::ComputeProgram> tonemap;
    Handle<gfx::ComputeProgram> build_histo;
    Handle<gfx::ComputeProgram> average_histo;

    Handle<gfx::Buffer> histogram;
    Handle<gfx::Image> average_luminance;
    TonemapOptions options = {};
};

struct PACKED GlobalUniform
{
    float4x4 camera_view;
    float4x4 camera_proj;
    float4x4 camera_view_inverse;
    float4x4 camera_projection_inverse;
    float4x4 camera_previous_view;
    float4x4 camera_previous_projection;
    float4   camera_position;
    u64 vertex_buffer_ptr;
    u64 primitive_buffer_ptr;
    float2   resolution;
    float delta_t;
    u32 frame_count;
    u32 camera_moved;
    u32 render_texture_offset;
    float2 jitter_offset;
    u32 is_path_tracing;
};

struct PACKED PushConstants
{
    u32 draw_idx = u32_invalid;
    u32 render_mesh_idx = u32_invalid;
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
    RenderTargets depth_only_rt;


    ImGuiPass imgui_pass;
    TonemapPass tonemap_pass;

    // Geometry pools
    Handle<gfx::GraphicsProgram> opaque_program;

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
