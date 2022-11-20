#pragma once
#include "exo/maths/numerics.h"
#include "exo/profile.h"

#include "cross/jobs/job.h"
#include "cross/jobs/waitable.h"

#include "exo/collections/span.h"

namespace cross
{
struct JobManager;

struct ForeachJob : public Job
{
	static constexpr u32 TASK_TYPE   = 0;
	exo::Span<u8>        user_range  = {};
	void                *user_lambda = nullptr;
	void                *user_data   = nullptr;

	// The indirection trough this callback makes it possible to type user_range and user_lambda in the function
	// creating the task.
	void (*callback)(ForeachJob &) = nullptr;

	volatile i64 *done_counter;
};

template <typename T>
using ForEachFn = void (*)(T &);
template <typename ElementType>
std::unique_ptr<Waitable> parallel_foreach(
	const JobManager &jobmanager, exo::Span<ElementType> values, ForEachFn<ElementType> lambda, int grain_size = 1024)
{
	EXO_PROFILE_SCOPE
	int chunks = (int(values.len()) + grain_size - 1) / grain_size;

	auto waitable = std::make_unique<Waitable>();
	waitable->jobs.reserve(std::size_t(chunks));

	for (int i_chunk = 0; i_chunk < chunks; ++i_chunk) {
		EXO_PROFILE_SCOPE_NAMED("Prepare chunk")

		exo::Span<ElementType> chunk =
			exo::Span(values.begin() + i_chunk * grain_size, values.begin() + (i_chunk + 1) * grain_size);

		auto job         = std::make_shared<ForeachJob>(ForeachJob{.done_counter = &waitable->jobs_finished});
		job->type        = ForeachJob::TASK_TYPE;
		job->user_range  = std::bit_cast<exo::Span<u8>>(chunk);
		job->user_lambda = (void *)lambda;

		job->callback = [](ForeachJob &job) {
			EXO_PROFILE_SCOPE_NAMED("User foreach job")
			auto casted_lambda = (ForEachFn<ElementType>)(job.user_lambda);
			auto casted_span   = std::bit_cast<exo::Span<ElementType>>(job.user_range);

			for (auto &element : casted_span) {
				casted_lambda(element);
			}
		};

		jobmanager.queue_job(*job);
		waitable->jobs.push(std::move(job));
	}

	return waitable;
}

// Foreach with user data
template <typename T, typename U>
using ForEachUserDataFn = void (*)(T &, U *);

template <typename ElementType, typename UserData, bool UseCurrentThread = false>
std::unique_ptr<Waitable> parallel_foreach_userdata(const JobManager &jobmanager,
	exo::Span<ElementType>                                            values,
	UserData                                                         *user_data,
	ForEachUserDataFn<ElementType, UserData>                          lambda,
	int                                                               grain_size = 1024)
{
	EXO_PROFILE_SCOPE
	int chunks = (int(values.len()) + grain_size - 1) / grain_size;

	auto waitable = std::make_unique<Waitable>();
	waitable->jobs.reserve(std::size_t(chunks));

	int i_chunk = 0;
	if constexpr (UseCurrentThread) {
		i_chunk = 1;
	}

	for (; i_chunk < chunks; ++i_chunk) {
		EXO_PROFILE_SCOPE_NAMED("Prepare chunk")

		auto end   = (i_chunk + 1) * grain_size;
		auto chunk = exo::Span(values.begin() + i_chunk * grain_size,
			values.begin() + (end > i32(values.len()) ? i32(values.len()) : end));

		auto job         = std::make_shared<ForeachJob>(ForeachJob{.done_counter = &waitable->jobs_finished});
		job->type        = ForeachJob::TASK_TYPE;
		job->user_range  = std::bit_cast<exo::Span<u8>>(chunk);
		job->user_lambda = (void *)lambda;
		job->user_data   = (void *)(user_data);

		job->callback = [](ForeachJob &job) {
			EXO_PROFILE_SCOPE_NAMED("User foreach job")
			auto casted_lambda   = (ForEachUserDataFn<ElementType, UserData>)(job.user_lambda);
			auto casted_span     = std::bit_cast<exo::Span<ElementType>>(job.user_range);
			auto casted_userdata = static_cast<UserData *>(job.user_data);

			for (auto &element : casted_span) {
				casted_lambda(element, casted_userdata);
			}
		};

		jobmanager.queue_job(*job);
		waitable->jobs.push(std::move(job));
	}

	if constexpr (UseCurrentThread) {
		EXO_PROFILE_SCOPE_NAMED("User foreach job")
		auto chunk0 = exo::Span(values.begin(),
			values.begin() + (grain_size > i32(values.len()) ? i32(values.len()) : grain_size));

		for (auto &element : chunk0) {
			lambda(element, user_data);
		}
	}

	return waitable;
}

} // namespace cross
