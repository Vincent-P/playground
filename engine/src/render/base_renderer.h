#pragma once
#include <exo/memory/string_repository.h>

#include "render/vulkan/context.h"
#include "render/vulkan/device.h"
#include "render/vulkan/surface.h"

#include "render/ring_buffer.h"
#include "render/render_timings.h"

namespace gfx = vulkan;

namespace cross { struct Window; }

inline constexpr uint FRAME_QUEUE_LENGTH  = 2;

struct BaseRenderer
{
    StringRepository str_repo = {};
    cross::Window *window = nullptr;

    gfx::Context context = {};
    gfx::Device device = {};
    gfx::Surface surface = {};

    uint frame_count = {};
    std::array<gfx::WorkPool, FRAME_QUEUE_LENGTH> work_pools = {};
    std::array<RenderTimings, FRAME_QUEUE_LENGTH> timings = {};
    gfx::Fence fence = {};

    RingBuffer dynamic_uniform_buffer = {};
    RingBuffer dynamic_vertex_buffer = {};
    RingBuffer dynamic_index_buffer = {};

    Handle<gfx::Image> empty_image = {};

    /// ---

    static BaseRenderer create(cross::Window &window, gfx::DeviceDescription desc);
    void destroy();

    BaseRenderer() = default;

    BaseRenderer(const BaseRenderer &other) = delete;
    BaseRenderer& operator=(const BaseRenderer &other) = delete;

    BaseRenderer(BaseRenderer && other) = default;
    BaseRenderer& operator=(BaseRenderer && other) = default;

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
