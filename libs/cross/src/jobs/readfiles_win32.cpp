#include "cross/jobs/readfiles.h"

#include <exo/macros/assert.h>
#include <exo/profile.h>

#include "cross/jobs/waitable.h"

#include "jobmanager_win32.h"
#include "jobs/job_win32.h"
#include "jobs/readfiles_win32.h"

#include "utils_win32.h"

#include <windows.h>

namespace cross
{
std::unique_ptr<Waitable> read_files(const JobManager &jobmanager, std::span<const ReadFileJobDesc> job_descs)
{
	auto &manager_impl = jobmanager.impl.get();

	auto waitable = std::make_unique<Waitable>();
	waitable->jobs.reserve(job_descs.size());

	for (const auto &job_desc : job_descs) {
		EXO_PROFILE_SCOPE_NAMED("Prepare job")

		auto  job          = std::make_shared<ReadFileJob>(ReadFileJob{.done_counter = &waitable->jobs_finished});
		auto &job_impl     = job->job_impl.get();
		auto &readjob_impl = job->readfilejob_impl.get();

		job->type = ReadFileJob::TASK_TYPE;
		job->path = job_desc.path;
		job->size = job_desc.size;
		job->dst  = job_desc.dst;

		job_impl.ovl.Offset     = DWORD(job_desc.offset);
		job_impl.ovl.OffsetHigh = DWORD(job_desc.offset >> 32);

		{
			EXO_PROFILE_SCOPE_NAMED("Open file")
			auto filepath            = utils::utf8_to_utf16(job_desc.path);
			readjob_impl.file_handle = CreateFile(filepath.c_str(),
				GENERIC_READ,
				0,
				nullptr,
				OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING,
				nullptr);
		}

		{
			EXO_PROFILE_SCOPE_NAMED("CreateIoCompletionPort")
			auto completion_port = CreateIoCompletionPort(readjob_impl.file_handle,
				manager_impl.completion_port,
				(ULONG_PTR)readjob_impl.file_handle,
				0);
			ASSERT(completion_port != nullptr);
		}
		{
			EXO_PROFILE_SCOPE_NAMED("PostQueuedCompletionStatus")
			auto res = PostQueuedCompletionStatus(manager_impl.completion_port, 0, NULL, &job_impl.ovl);
			ASSERT(res);
		}

		waitable->jobs.push_back(std::move(job));
	}

	return waitable;
}
} // namespace cross
