#include "cross/jobmanager.h"

#include <exo/macros/assert.h>
#include <exo/profile.h>

#include "cross/jobs/foreach.h"
#include "cross/jobs/readfiles.h"

#include "jobmanager_win32.h" // for Impls
#include "jobs/job_win32.h"
#include "jobs/readfiles_win32.h"

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

	impl.completion_port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, NULL, THREAD_POOL_LENGTH);

	// Initialize threads
	for (auto &thread : jobmanager.threads) {
		auto &thread_impl = thread.impl.get();

		void *thread_param = impl.completion_port;
		auto *thread_id    = &thread_impl.id;
		thread_impl.handle = CreateThread(nullptr, 0, worker_thread_proc, thread_param, 0, thread_id);
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

	for (auto &thread : this->threads) {
		auto &thread_impl = thread.impl.get();
		CloseHandle(thread_impl.handle);
		thread_impl.handle = nullptr;
	}
}

void worker_thread_read_file(ReadFileJob &job)
{
	auto &readjob_impl = job.readfilejob_impl.get();

	auto *complete_job     = new ReadFileCompletedJob;
	auto &completejob_impl = complete_job->job_impl.get();

	complete_job->type         = ReadFileCompletedJob::TASK_TYPE;
	complete_job->path         = job.path;
	complete_job->read_size    = job.size;
	complete_job->done_counter = job.done_counter;
	completejob_impl.ovl       = job.job_impl.get().ovl;

	auto res = ReadFile(readjob_impl.file_handle, job.dst.data(), DWORD(job.size), nullptr, &completejob_impl.ovl);

	auto last_error = GetLastError();
	ASSERT(!res && last_error == ERROR_IO_PENDING);
}

DWORD worker_thread_proc(void *param)
{
	HANDLE completion_port = param;
	// DWORD  thread_id       = GetCurrentThreadId();

	unsigned long bytes_transferred = 0;
	ULONG_PTR     completion_key    = NULL;
	LPOVERLAPPED  overlapped        = nullptr;
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
			auto &readfile_job = *reinterpret_cast<ReadFileJob *>(p_job);
			worker_thread_read_file(readfile_job);
		} else if (p_job->type == ReadFileCompletedJob::TASK_TYPE) {
			auto readcomplete_job = reinterpret_cast<ReadFileCompletedJob *>(p_job);
			ASSERT(readcomplete_job->read_size >= bytes_transferred);
			InterlockedIncrement64(readcomplete_job->done_counter);
			delete readcomplete_job;

			auto file_handle = (HANDLE)(completion_key);
			CloseHandle(file_handle);
		} else {
			ASSERT(false);
		}
	}
	return 0;
}

}; // namespace cross
