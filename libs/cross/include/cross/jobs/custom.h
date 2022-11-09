#pragma once
#include <exo/maths/numerics.h>

#include "cross/jobmanager.h"
#include "cross/jobs/job.h"
#include "cross/jobs/waitable.h"

#include <span>

namespace cross
{
struct CustomJob : public Job
{
	static constexpr u32 TASK_TYPE   = 3;
	void                *user_lambda = nullptr;
	void                *user_data   = nullptr;

	// The indirection trough this callback makes it possible to type user_range and user_lambda in the function
	// creating the task.
	void (*callback)(CustomJob &) = nullptr;

	volatile i64 *done_counter;
};

template <typename UserData>
using UserLambda = void (*)(UserData *);

template <typename UserData, bool UseCurrentThread = false>
std::unique_ptr<Waitable> custom_job(const JobManager &jobmanager, UserData *user_data, UserLambda<UserData> lambda)
{
	auto waitable = std::make_unique<Waitable>();
	waitable->jobs.reserve(1);

	auto job         = std::make_shared<CustomJob>(CustomJob{.done_counter = &waitable->jobs_finished});
	job->type        = CustomJob::TASK_TYPE;
	job->user_lambda = (void *)lambda;
	job->user_data   = (void *)(user_data);

	job->callback = [](CustomJob &job) {
		auto casted_lambda   = (UserLambda<UserData>)(job.user_lambda);
		auto casted_userdata = static_cast<UserData *>(job.user_data);
		casted_lambda(casted_userdata);
	};

	jobmanager.queue_job(*job);
	waitable->jobs.push_back(std::move(job));

	return waitable;
}

} // namespace cross
