#pragma once
#include "cross/jobs/job.h"
#include <exo/forward_container.h>
#include <exo/maths/numerics.h>

#include <memory>
#include "exo/collections/span.h"
#include <string_view>

namespace cross
{
struct JobManager;
struct Waitable;

struct ReadFileJob : public Job
{
	static constexpr u32 TASK_TYPE   = 1;
	exo::Span<u8>        user_data   = {};
	void                *user_lambda = nullptr;
	struct Impl;
	exo::ForwardContainer<Impl> readfilejob_impl;

	std::string_view path;
	std::size_t      size;
	exo::Span<u8>    dst;

	volatile i64 *done_counter;
};

struct ReadFileJobDesc
{
	std::string_view path;
	exo::Span<u8>    dst;
	std::size_t      offset = 0;
	std::size_t      size   = 0;
};

std::unique_ptr<Waitable> read_files(const JobManager &jobmanager, exo::Span<const ReadFileJobDesc> jobs);
} // namespace cross
