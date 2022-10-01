#pragma once
#include "cross/jobmanager.h"
#include <windows.h>

namespace cross
{
struct Thread::Impl
{
	HANDLE handle = {};
	DWORD  id     = {};
};

struct JobManager::Impl
{
	HANDLE completion_port = {};
};
} // namespace cross
