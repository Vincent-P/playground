#pragma once
#include "cross/jobs/job.h"
#include "cross/jobs/readfiles.h"

#include <windows.h>

namespace cross
{
struct ReadFileJob::Impl
{
	HANDLE file_handle;
};

struct ReadFileCompletedJob : Job
{
	static constexpr int TASK_TYPE = 2;
	exo::StringView     path;
	std::size_t          read_size = 0;

	volatile i64 *done_counter;
};
} // namespace cross
