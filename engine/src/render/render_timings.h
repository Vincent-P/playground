#pragma once

#include <exo/time.h>
#include <exo/maths/numerics.h>
#include <exo/collections/vector.h>

#include "render/vulkan/queries.h"

#include <string>

namespace vulkan { struct Device;  struct Work;};
namespace gfx = vulkan;
struct StringRepository;

inline constexpr u32 TIMESTAMPS_PER_FRAME = 16;

struct RenderTimings
{
    StringRepository *str_repo = nullptr;
    Vec<const char*> labels = {};
    Vec<double> cpu = {};
    Vec<double> gpu = {};

    Vec<u64> gpu_ticks = {};
    Vec<Timepoint> cpu_ticks = {};
    u32 current_query = 0;
    bool began = false;
    gfx::QueryPool pool;

    static RenderTimings create(gfx::Device &d, StringRepository *r);
    void destroy(gfx::Device &device);
    void begin_label(gfx::Work &work, const char *label);
    void end_label(gfx::Work &cmd);
    void get_results(gfx::Device &device);
    void reset(gfx::Device &device);
};
