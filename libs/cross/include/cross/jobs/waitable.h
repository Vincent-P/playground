#pragma once
#include <exo/collections/vector.h>
#include <exo/maths/numerics.h>

#include <memory>

namespace cross
{
struct Job;
struct Waitable
{
	Vec<std::shared_ptr<Job>> jobs          = {};
	volatile i64              jobs_finished = 0;

	void wait();
	bool is_done();
};
} // namespace cross
