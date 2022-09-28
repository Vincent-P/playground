#include <exo/collections/span.h>
#include <exo/macros/assert.h>
#include <exo/macros/defer.h>
#include <exo/path.h>

#include <windows.h>

#include <intrin.h>
#include <synchapi.h>

#include <any>
#include <atomic>
#include <cstdio>
#include <span>
#include <vector>

namespace utils
{
std::wstring utf8_to_utf16(const std::string_view &str)
{
	if (str.empty()) {
		return {};
	}

	int res = MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), nullptr, 0);
	ASSERT(res > 0);
	auto size_needed = static_cast<usize>(res);
	// TODO: Remove allocation
	std::wstring result(size_needed, 0);
	MultiByteToWideChar(CP_UTF8,
		0,
		str.data(),
		static_cast<int>(str.size()),
		result.data(),
		static_cast<int>(result.size()));
	return result;
}
} // namespace utils

constexpr std::size_t WORKER_THREAD_POOL_LENGTH = 8;

struct JobManager
{
	HANDLE completion_port                       = {};
	HANDLE threads[WORKER_THREAD_POOL_LENGTH]    = {};
	DWORD  thread_ids[WORKER_THREAD_POOL_LENGTH] = {};
};

enum struct TaskType
{
	Waitable,
	ReadFile,
	Invalid
};

struct TaskHeader
{
	OVERLAPPED ovl  = {};
	TaskType   type = TaskType::Invalid;
};

template <typename Task> struct Waitable
{
	std::vector<Task> tasks     = {};
	std::atomic<int>  task_done = 0;

	void wait()
	{
		int tasks_length = int(tasks.size());
		while (true) {
			if (task_done.load() >= tasks_length) {
				break;
			}
		}
	}
};

namespace tasks
{
struct WaitableTask
{
	OVERLAPPED ovl         = {};
	TaskType   type        = TaskType::Waitable;
	std::any   user_data   = {};
	void      *user_lambda = nullptr;

	// The indirection trough this callback makes it possible to type user_data and user_lambda in the function creating
	// the task.
	void (*callback)(std::any &, void *) = nullptr;

	std::atomic<int> &done_counter;
};

template <typename T> using ForEachFn = void (*)(T &);

template <typename ElementType>
Waitable<WaitableTask> *parallel_foreach(
	const JobManager &jobmanager, std::span<ElementType> values, ForEachFn<ElementType> lambda, int grain_size = 1024)
{
	int chunks = (int(values.size()) + grain_size - 1) / grain_size;

	auto *waitable = new Waitable<WaitableTask>();
	waitable->tasks.reserve(std::size_t(chunks));

	for (int i_chunk = 1; i_chunk < chunks; ++i_chunk) {
		WaitableTask task = {.done_counter = waitable->task_done};
		task.user_data    = std::make_any<std::span<ElementType>>(values.begin() + i_chunk * grain_size,
            values.begin() + (i_chunk + 1) * grain_size);

		task.user_lambda = (void *)lambda;

		task.callback = [](std::any &user_data, void *user_lambda) {
			auto casted_lambda = (ForEachFn<ElementType>)(user_lambda);
			auto casted_span   = std::any_cast<std::span<ElementType>>(user_data);
			for (auto &element : casted_span) {
				casted_lambda(element);
			}
		};

		waitable->tasks.push_back(std::move(task));

		auto task_index = std::size_t(i_chunk - 1);
		auto res = PostQueuedCompletionStatus(jobmanager.completion_port, 0, NULL, &waitable->tasks[task_index].ovl);
		ASSERT(res);
	}

	auto first_chunk = std::span<ElementType>(values.begin(), values.begin() + int(grain_size));
	for (auto &element : first_chunk) {
		lambda(element);
	}
	waitable->task_done++;

	return waitable;
}

struct ReadFileTask
{
	OVERLAPPED ovl         = {};
	TaskType   type        = TaskType::ReadFile;
	HANDLE     file_handle = {};

	std::string_view path;
	std::size_t      size;
	std::span<u8>    dst;

	std::atomic<int> &done_counter;
};

struct ReadFileJob
{
	std::string_view path;
	std::size_t      offset;
	std::size_t      size;
	std::span<u8>    dst;
};

Waitable<ReadFileTask> *read_files(const JobManager &jobmanager, std::span<const ReadFileJob> jobs)
{
	auto *waitable = new Waitable<ReadFileTask>();

	waitable->tasks.reserve(jobs.size());

	for (const auto &job : jobs) {
		ReadFileTask task = {.done_counter = waitable->task_done};
		task.path         = job.path;
		task.size         = job.size;
		task.dst          = job.dst;

		task.ovl.Offset     = DWORD(job.offset);
		task.ovl.OffsetHigh = DWORD(job.offset >> 32);

		auto filepath    = utils::utf8_to_utf16(job.path);
		task.file_handle = CreateFile(filepath.c_str(),
			GENERIC_READ,
			FILE_SHARE_READ,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
			NULL);

		waitable->tasks.push_back(std::move(task));

		auto completion_port = CreateIoCompletionPort(task.file_handle, jobmanager.completion_port, NULL, 0);
		ASSERT(completion_port != NULL);

		auto res = ReadFile(task.file_handle, job.dst.data(), DWORD(job.size), NULL, &waitable->tasks.back().ovl);
		ASSERT(!res && GetLastError() == ERROR_IO_PENDING);
	}

	return waitable;
}
} // namespace tasks

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

		auto *p_task_header = (TaskHeader *)overlapped;

		if (p_task_header->type == TaskType::Waitable) {
			auto &task = *(tasks::WaitableTask *)(p_task_header);
			task.callback(task.user_data, task.user_lambda);
			task.done_counter++;
		} else if (p_task_header->type == TaskType::ReadFile) {
			auto &task = *(tasks::ReadFileTask *)(p_task_header);
			ASSERT(task.size >= bytes_transferred);
			printf("Read file %.*s\n", int(task.path.size()), task.path.data());
			task.done_counter++;
		} else {
			ASSERT(false);
		}
	}
	return 0;
}

void jobmanager_init(JobManager &jobmanager)
{
	// Initialize the completion port (task queue)
	std::uint32_t nb_concurrent_threads = WORKER_THREAD_POOL_LENGTH;
	jobmanager.completion_port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, nb_concurrent_threads);

	// Initialize threads
	for (std::size_t i_thread = 0; i_thread < WORKER_THREAD_POOL_LENGTH; ++i_thread) {
		void *thread_param           = jobmanager.completion_port;
		auto *thread_id              = &jobmanager.thread_ids[i_thread];
		jobmanager.threads[i_thread] = CreateThread(NULL, 0, worker_thread_proc, thread_param, 0, thread_id);
		ASSERT(jobmanager.threads[i_thread]);
	}
}

void jobmanager_deinit(JobManager &jobmanager)
{
	CloseHandle(jobmanager.completion_port);

	WaitForMultipleObjects(WORKER_THREAD_POOL_LENGTH, jobmanager.threads, TRUE, INFINITE);

	for (std::size_t i_thread = 0; i_thread < WORKER_THREAD_POOL_LENGTH; ++i_thread) {
		CloseHandle(jobmanager.threads[i_thread]);
	}
}

int main(int argc, char *argv[])
{
	JobManager jobmanager;
	jobmanager_init(jobmanager);

	// Create a dummy vector
	std::vector<int> values;
	values.reserve(1 << 20);
	for (int i = 0; i < (1 << 20); ++i) {
		values.push_back(i);
	}

	auto waitable_foreach = tasks::parallel_foreach<int>(jobmanager, std::span(values), [](int &value) { value *= 2; });

	auto before_wait = __rdtsc();
	waitable_foreach->wait();
	auto after_wait = __rdtsc();
	printf("waited for %llu cycles for the task to finish\n", after_wait - before_wait);

	printf("first few values:\n");
	for (std::size_t i_el = 0; i_el < values.size(); i_el += 1024) {
		printf("values[%zu] = %d\n", i_el, values[i_el]);
	}

	void		 *gpu_buffer      = std::malloc(256 << 20);
	std::span<u8> gpu_upload_area = std::span<u8>((u8 *)gpu_buffer, 256 << 20);

	std::vector<tasks::ReadFileJob> read_jobs;
	read_jobs.resize(std::size_t(argc) - 1);
	for (std::size_t i = 0, end = std::size_t(argc) - 1; i < end; ++i) {
		read_jobs[i].path = std::string_view(argv[i + 1]);
		read_jobs[i].dst  = gpu_upload_area;
		read_jobs[i].size = 256u << 20;
	}
	auto waitable_files = tasks::read_files(jobmanager, read_jobs);
	waitable_files->wait();

	jobmanager_deinit(jobmanager);

	return 0;
}
