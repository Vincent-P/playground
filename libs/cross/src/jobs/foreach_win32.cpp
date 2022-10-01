#include "cross/jobmanager.h"

#include <exo/macros/assert.h>
#include <exo/profile.h>

#include "cross/jobs/foreach.h"

#include "job_win32.h"        // for Impl
#include "jobmanager_win32.h" // for Impl

namespace cross
{
void queue_parallel_foreach_job(const JobManager &manager, Job &job)
{
	EXO_PROFILE_SCOPE_NAMED("PostQueuedCompletionStatus")
	const auto &manager_impl = manager.impl.get();
	auto       &job_impl     = job.job_impl.get();

	auto res = PostQueuedCompletionStatus(manager_impl.completion_port, 0, NULL, &job_impl.ovl);
	ASSERT(res);
}
} // namespace cross
