#pragma once
#include <exo/maths/numerics.h>
#include <exo/profile.h>

#include "cross/jobs/job.h"
#include "cross/jobs/waitable.h"

#include <span>

namespace cross
{
struct JobManager;

void queue_parallel_foreach_job(const JobManager &manager, Job &job);

struct ForeachJob : public Job
{
	static constexpr u32 TASK_TYPE   = 0;
	std::span<u8>        user_data   = {};
	void                *user_lambda = nullptr;

	// The indirection trough this callback makes it possible to type user_data and user_lambda in the function creating
	// the task.
	void (*callback)(ForeachJob &) = nullptr;

	volatile i64 *done_counter;
};

template <typename T> using ForEachFn = void (*)(T &);
template <typename ElementType>
std::unique_ptr<Waitable> parallel_foreach(
	const JobManager &jobmanager, std::span<ElementType> values, ForEachFn<ElementType> lambda, int grain_size = 1024)
{
	EXO_PROFILE_SCOPE
	int chunks = (int(values.size()) + grain_size - 1) / grain_size;

	auto waitable = std::make_unique<Waitable>();
	waitable->jobs.reserve(std::size_t(chunks));

	for (int i_chunk = 0; i_chunk < chunks; ++i_chunk) {
		EXO_PROFILE_SCOPE_NAMED("Prepare chunk")

		auto chunk = std::span(values.begin() + i_chunk * grain_size, values.begin() + (i_chunk + 1) * grain_size);

		auto job         = std::make_shared<ForeachJob>(ForeachJob{.done_counter = &waitable->jobs_finished});
		job->type        = ForeachJob::TASK_TYPE;
		job->user_data   = std::bit_cast<std::span<u8>>(chunk);
		job->user_lambda = (void *)lambda;

		job->callback = [](ForeachJob &job) {
			EXO_PROFILE_SCOPE_NAMED("User foreach job")
			auto casted_lambda = (ForEachFn<ElementType>)(job.user_lambda);
			auto casted_span   = std::bit_cast<std::span<ElementType>>(job.user_data);

			for (auto &element : casted_span) {
				casted_lambda(element);
			}
		};

		queue_parallel_foreach_job(jobmanager, *job);
		waitable->jobs.push_back(std::move(job));
	}

	return waitable;
}
} // namespace cross
