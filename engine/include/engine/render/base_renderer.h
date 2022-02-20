#pragma once
#include <exo/memory/string_repository.h>

#include "engine/render/vulkan/context.h"
#include "engine/render/vulkan/device.h"
#include "engine/render/vulkan/surface.h"

#include "engine/render/ring_buffer.h"
#include "engine/render/render_timings.h"
#include "engine/render/streamer.h"
#include "engine/render/vulkan/descriptor_set.h"

#include <array>

namespace gfx = vulkan;
namespace exo { struct Window; }
namespace exo { struct ScopeStack; }

inline constexpr uint FRAME_QUEUE_LENGTH  = 2;

struct BaseRenderer
{
    static BaseRenderer *create(exo::ScopeStack &scope, exo::Window *window, gfx::DeviceDescription desc);
    ~BaseRenderer();

    // Ring buffer uniforms
    void *bind_compute_shader_options(gfx::ComputeWork &cmd, usize options_len);
    void *bind_graphics_shader_options(gfx::GraphicsWork &cmd, usize options_len);
    template<typename T>
    T *bind_compute_shader_options(gfx::ComputeWork &cmd) { return (T*)bind_compute_shader_options(cmd, sizeof(T)); }
    template<typename T>
    T *bind_graphics_shader_options(gfx::GraphicsWork &cmd) { return (T*)bind_graphics_shader_options(cmd, sizeof(T)); }

    void *bind_global_options(gfx::GraphicsWork &cmd, usize options_len);
    template<typename T>
    T *bind_global_options(gfx::GraphicsWork &cmd) { return (T*)bind_global_options(cmd, sizeof(T)); }

    void reload_shader(std::string_view shader_name);
    void on_resize();
    bool start_frame();
    bool end_frame(gfx::ComputeWork &cmd);

    /// ---

    exo::StringRepository str_repo = {};
    exo::Window *window = nullptr;

    gfx::Context context = {};
    gfx::Device device = {};
    gfx::Surface surface = {};

    uint frame_count = {};
    std::array<gfx::WorkPool, FRAME_QUEUE_LENGTH> work_pools = {};
    std::array<RenderTimings, FRAME_QUEUE_LENGTH> timings = {};
    gfx::Fence fence = {};

    RingBuffer dynamic_uniform_buffer = {};
	Vec<gfx::DynamicBufferDescriptor> dynamic_descriptors;
    RingBuffer dynamic_vertex_buffer = {};
    RingBuffer dynamic_index_buffer = {};

    Handle<gfx::Image> empty_image = {};

    Streamer streamer = {};
};
