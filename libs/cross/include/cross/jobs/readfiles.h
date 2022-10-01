#pragma once
#include "cross/jobs/job.h"
#include <exo/forward_container.h>
#include <exo/maths/numerics.h>

#include <memory>
#include <span>
#include <string_view>

namespace cross
{
struct JobManager;
struct Waitable;

struct ReadFileJob : public Job
{
	static constexpr u32 TASK_TYPE   = 1;
	std::span<u8>        user_data   = {};
	void                *user_lambda = nullptr;
	struct Impl;
	exo::ForwardContainer<Impl> readfilejob_impl;

	std::string_view path;
	std::size_t      size;
	std::span<u8>    dst;

	volatile i64 *done_counter;
};

struct ReadFileJobDesc
{
	std::string_view path;
	std::span<u8>    dst;
	std::size_t      offset = 0;
	std::size_t      size   = 0;
};

std::unique_ptr<Waitable> read_files(const JobManager &jobmanager, std::span<const ReadFileJobDesc> jobs);
} // namespace cross
