#pragma once
#include "render/vulkan/device.h"
#include "render/vulkan/surface.h"

#include "render/ring_buffer.h"
#include "render/render_timings.h"

namespace gfx = vulkan;

inline constexpr uint FRAME_QUEUE_LENGTH  = 2;
inline constexpr u32 TIMESTAMPS_PER_FRAME = 16;

struct RenderTargets
{
    Handle<gfx::RenderPass> clear_renderpass;
    Handle<gfx::RenderPass> load_renderpass;
    Handle<gfx::Framebuffer> framebuffer;
    Handle<gfx::Image> image;
    Handle<gfx::Image> depth;
};

struct BaseRenderer
{
    // Base renderer
    gfx::Context context;
    gfx::Device device;
    gfx::Surface surface;
    uint frame_count;
    std::array<gfx::WorkPool, FRAME_QUEUE_LENGTH> work_pools;
    std::array<RenderTimings, FRAME_QUEUE_LENGTH> timings;
    gfx::Fence fence;

    RingBuffer dynamic_uniform_buffer;
    RingBuffer dynamic_vertex_buffer;
    RingBuffer dynamic_index_buffer;

    Handle<gfx::Image> empty_sampled_image;
    Handle<gfx::Image> empty_storage_image;

    RenderTargets swapchain_rt;

    /// ---

    static BaseRenderer create(const platform::Window &window, gfx::DeviceDescription desc);
    void destroy();

    // Ring buffer uniforms
    void *bind_shader_options(gfx::ComputeWork &cmd, Handle<gfx::GraphicsProgram> program, usize options_len);
    void *bind_shader_options(gfx::ComputeWork &cmd, Handle<gfx::ComputeProgram> program, usize options_len);
    void *bind_global_options(usize options_len);
    template<typename T>
    T *bind_shader_options(gfx::ComputeWork &cmd, Handle<gfx::GraphicsProgram> program) { return (T*)bind_shader_options(cmd, program, sizeof(T)); }
    template<typename T>
    T *bind_shader_options(gfx::ComputeWork &cmd, Handle<gfx::ComputeProgram> program) { return (T*)bind_shader_options(cmd, program, sizeof(T)); }
    template<typename T>
    T *bind_global_options() { return (T*)bind_global_options(sizeof(T)); }

    void reload_shader(std::string_view shader_name);
    void on_resize();
    bool start_frame();
    bool end_frame(gfx::ComputeWork &cmd);
};
