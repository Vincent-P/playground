#pragma once

#include "base/handle.hpp"

#include "render/vulkan/context.hpp"
#include "render/vulkan/device.hpp"
#include "render/vulkan/resources.hpp"
#include "render/vulkan/surface.hpp"

namespace gfx = vulkan;

inline constexpr uint FRAME_QUEUE_LENGTH = 1;

namespace gltf {struct Model;}
namespace UI {struct Context;}

struct RenderMesh
{
    Handle<gltf::Model> model_handle;
    Vec<Handle<gfx::Image>> images;
    u32 vertices_offset;
    u32 indices_offset;
    u32 images_offset;
};

class Scene;

struct PACKED ImguiOptions
{
    float2 scale;
    float2 translation;
    u64 vertices_pointer;
    u32 texture_binding;
};

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
};

struct ImGuiPass
{
    Handle<gfx::GraphicsProgram> program;
    Handle<gfx::Image>  font_atlas;
    Handle<gfx::Buffer> font_atlas_staging;
    Handle<gfx::Buffer> vertices;
    Handle<gfx::Buffer> indices;
    Handle<gfx::Buffer> options;
};

struct TonemapPass
{
    Handle<gfx::ComputeProgram> tonemap;
    Handle<gfx::Buffer> tonemap_options;
    Handle<gfx::ComputeProgram> build_histo;
    Handle<gfx::Buffer> build_histo_options;
    Handle<gfx::ComputeProgram> average_histo;
    Handle<gfx::Buffer> average_histo_options;

    Handle<gfx::Buffer> histogram;
    Handle<gfx::Image> average_luminance;
};

struct Renderer
{
    Settings settings;

    // Base renderer
    gfx::Context context;
    gfx::Device device;
    gfx::Surface surface;
    uint frame_count;
    std::array<gfx::WorkPool, FRAME_QUEUE_LENGTH> work_pools;
    gfx::Fence fence;

    // User renderer
    RenderTargets hdr_rt;
    RenderTargets swapchain_rt;

    Handle<gfx::Image> hdr_buffer;

    gfx::Fence transfer_done;
    u64 transfer_fence_value = 0;

    ImGuiPass imgui_pass;
    TonemapPass tonemap_pass;

    /// ---

    static Renderer create(const platform::Window &window);
    void destroy();

    void on_resize();
    bool start_frame();
    bool end_frame(gfx::ComputeWork &cmd);
    void display_ui(UI::Context &ui);
    void update(const Scene &scene);
};

inline uint3 dispatch_size(uint3 image_size, uint threads_xy, uint threads_z = 1)
{
    return {
        .x = image_size.x / threads_xy + uint(image_size.x % threads_xy != 0),
        .y = image_size.y / threads_xy + uint(image_size.y % threads_xy != 0),
        .z = image_size.z / threads_z  + uint(image_size.z % threads_z  != 0),
    };
}
