#pragma once
#include <exo/forward_container.h>
#include <exo/maths/numerics.h>

namespace cross
{
inline constexpr usize THREAD_POOL_LENGTH = 24;

struct Job;

struct Thread
{
	struct Impl;
	exo::ForwardContainer<Impl> impl;
};

struct JobManager
{
	Thread threads[THREAD_POOL_LENGTH];

	struct Impl;
	exo::ForwardContainer<Impl> impl;

	static JobManager create();
	void              destroy();
};
} // namespace cross
