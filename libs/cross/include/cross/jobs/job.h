#pragma once
#include <exo/forward_container.h>
#include <exo/maths/numerics.h>

namespace cross
{
struct Job
{
	struct Impl;
	exo::ForwardContainer<Impl> job_impl;
	u32                         type = u32_invalid;
};
} // namespace cross
