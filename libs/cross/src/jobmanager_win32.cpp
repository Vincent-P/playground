#include "cross/jobmanager.h"

#include <exo/macros/assert.h>
#include <exo/profile.h>

#include "cross/jobs/foreach.h"
#include "cross/jobs/readfiles.h"

#include "jobmanager_win32.h" // for Impls

#include <cstdio>
#include <windows.h>

namespace cross
{
DWORD      worker_thread_proc(void *param);
JobManager JobManager::create()
{
	EXO_PROFILE_SCOPE

	JobManager jobmanager;
	auto      &impl = jobmanager.impl.get();

	impl.completion_port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, THREAD_POOL_LENGTH);

	// Initialize threads
	for (usize i_thread = 0; i_thread < THREAD_POOL_LENGTH; ++i_thread) {
		auto &thread_impl = jobmanager.threads[i_thread].impl.get();

		void *thread_param = impl.completion_port;
		auto *thread_id    = &thread_impl.id;
		thread_impl.handle = CreateThread(NULL, 0, worker_thread_proc, thread_param, 0, thread_id);
		ASSERT(thread_impl.handle);
	}
	return jobmanager;
}

void JobManager::destroy()
{
	EXO_PROFILE_SCOPE

	auto &manager_impl = this->impl.get();

	CloseHandle(manager_impl.completion_port);

	HANDLE thread_handles[THREAD_POOL_LENGTH];
	for (usize i_thread = 0; i_thread < THREAD_POOL_LENGTH; ++i_thread) {
		const auto &thread_impl  = this->threads[i_thread].impl.get();
		thread_handles[i_thread] = thread_impl.handle;
	}

	WaitForMultipleObjects(THREAD_POOL_LENGTH, thread_handles, TRUE, INFINITE);

	for (usize i_thread = 0; i_thread < THREAD_POOL_LENGTH; ++i_thread) {
		auto &thread_impl = this->threads[i_thread].impl.get();
		CloseHandle(thread_impl.handle);
		thread_impl.handle = NULL;
	}
}

DWORD worker_thread_proc(void *param)
{
	HANDLE completion_port = param;
	// DWORD  thread_id       = GetCurrentThreadId();

	unsigned long bytes_transferred = 0;
	ULONG_PTR     completion_key    = NULL;
	LPOVERLAPPED  overlapped        = NULL;
	BOOL          res;

	while (true) {
		res = GetQueuedCompletionStatus(completion_port, &bytes_transferred, &completion_key, &overlapped, INFINITE);
		if (!overlapped || !res) {
			break;
		}

		EXO_PROFILE_SCOPE_NAMED("Job execution")
		auto *p_job = (Job *)overlapped;
		ASSERT(p_job->type != u32_invalid);
		if (p_job->type == ForeachJob::TASK_TYPE) {
			auto &foreachjob = *reinterpret_cast<ForeachJob *>(p_job);
			foreachjob.callback(foreachjob);
			InterlockedIncrement64(foreachjob.done_counter);
		} else if (p_job->type == ReadFileJob::TASK_TYPE) {
			auto &foreachjob = *reinterpret_cast<ReadFileJob *>(p_job);
			ASSERT(foreachjob.size >= bytes_transferred);
			// printf("Read file %.*s\n", int(foreachjob.path.size()), foreachjob.path.data());
			InterlockedIncrement64(foreachjob.done_counter);
		} else {
			ASSERT(false);
		}
	}
	return 0;
}

}; // namespace cross
