#pragma once

#include "cross/jobs/job.h"

#include <windows.h>

namespace cross
{
struct Job::Impl
{
	OVERLAPPED ovl = {};
};
} // namespace cross
