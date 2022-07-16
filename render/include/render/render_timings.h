#pragma once

#include <exo/collections/vector.h>
#include <exo/maths/numerics.h>
#include <exo/time.h>

#include "render/vulkan/queries.h"

#include <string>

namespace vulkan
{
struct Device;
struct Work;
}; // namespace vulkan
namespace gfx = vulkan;
namespace exo
{
struct StringRepository;
}

inline constexpr u32 TIMESTAMPS_PER_FRAME = 32;

struct RenderTimings
{
	exo::StringRepository *str_repo = nullptr;
	Vec<const char *>      labels   = {};
	Vec<double>            cpu      = {};
	Vec<double>            gpu      = {};

	Vec<u64>            gpu_ticks     = {};
	Vec<exo::Timepoint> cpu_ticks     = {};
	u32                 current_query = 0;
	bool                began         = false;
	gfx::QueryPool      pool;

	static RenderTimings create(gfx::Device &d, exo::StringRepository *r);
	void                 destroy(gfx::Device &device);
	void                 begin_label(gfx::Work &work, const char *label);
	void                 end_label(gfx::Work &cmd);
	void                 get_results(gfx::Device &device);
	void                 reset(gfx::Device &device);
};
