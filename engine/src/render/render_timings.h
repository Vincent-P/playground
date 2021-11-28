#pragma once

#include <exo/time.h>
#include <exo/maths/numerics.h>
#include <exo/collections/vector.h>

#include "render/vulkan/queries.h"

#include <string>

namespace vulkan { struct Device;  struct Work;};
namespace gfx = vulkan;

inline constexpr u32 TIMESTAMPS_PER_FRAME = 16;

struct RenderTimings
{
    Vec<const char*> labels = {};
    Vec<double> cpu = {};
    Vec<double> gpu = {};

    Vec<u64> gpu_ticks = {};
    Vec<Timepoint> cpu_ticks = {};
    u32 current_query = 0;
    bool began = false;
    gfx::QueryPool pool;

    void create(gfx::Device &device);
    void destroy(gfx::Device &device);
    void begin_label(gfx::Work &work, const char *label);
    void end_label(gfx::Work &cmd);
    void get_results(gfx::Device &device);
    void reset(gfx::Device &device);
};
