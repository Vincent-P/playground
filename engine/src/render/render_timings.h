#pragma once

#include <exo/types.h>
#include <exo/collections/vector.h>

#include "render/vulkan/queries.h"

#include <string>
#include <chrono>

using Clock = std::chrono::high_resolution_clock;
using Timepoint = std::chrono::time_point<Clock>;
namespace vulkan { struct Device;  struct Work;};
namespace gfx = vulkan;

inline constexpr u32 TIMESTAMPS_PER_FRAME = 16;

struct RenderTimings
{
    Vec<std::string> labels = {};
    Vec<double> cpu = {};
    Vec<double> gpu = {};

    Vec<u64> gpu_ticks = {};
    Vec<Timepoint> cpu_ticks = {};
    u32 current_query = 0;
    bool began = false;
    gfx::QueryPool pool;

    void create(gfx::Device &device);
    void destroy(gfx::Device &device);
    void begin_label(gfx::Work &work, std::string &&label);
    void end_label(gfx::Work &cmd);
    void get_results(gfx::Device &device);
    void reset(gfx::Device &device);
};
