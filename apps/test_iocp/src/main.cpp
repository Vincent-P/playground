#include <exo/collections/span.h>
#include <exo/profile.h>

#include <cross/jobmanager.h>
#include <cross/jobs/foreach.h>
#include <cross/jobs/readfiles.h>

#include <algorithm>
#include <cstdio>
#include <execution>

#include <intrin.h>
#include <windows.h>

inline constexpr usize VECTOR_SIZE = 1 << 17;

void test_async(int argc, char *argv[])
{
	EXO_PROFILE_SCOPE
	auto manager = cross::JobManager::create();

	{
		EXO_PROFILE_SCOPE_NAMED("foreach test")
		std::vector<int> values;
		{
			EXO_PROFILE_SCOPE_NAMED("vector creating")
			values.reserve(VECTOR_SIZE);
			for (u32 i = 0; i < VECTOR_SIZE; ++i) {
				values.push_back(i32(i));
			}
		}

		auto w = cross::parallel_foreach<int>(manager, std::span(values), [](int &value) {
			EXO_PROFILE_SCOPE_NAMED("Expensive int calculation")
			double accum = 0;
			for (int i = 0; i < value; ++i) {
				accum += sqrt(double(i));
			}
			value = int(accum);
		});

		auto before_wait = __rdtsc();
		w->wait();
		auto after_wait = __rdtsc();
		printf("waited for %llu cycles for the tasks to finish\n", after_wait - before_wait);
	}

	{
		EXO_PROFILE_SCOPE_NAMED("read files test")
		std::vector<cross::ReadFileJobDesc> read_jobs;
		void                               *gpu_buffer      = nullptr;
		std::span<u8>                       gpu_upload_area = {};
		{
			EXO_PROFILE_SCOPE_NAMED("prepare dst buffer")
			gpu_buffer      = std::malloc(256 << 20);
			gpu_upload_area = std::span<u8>((u8 *)gpu_buffer, 256 << 20);
		}
		{
			EXO_PROFILE_SCOPE_NAMED("prepare read jobs")
			read_jobs.resize(std::size_t(argc) - 1);
			for (std::size_t i = 0, end = std::size_t(argc) - 1; i < end; ++i) {
				read_jobs[i].path = std::string_view(argv[i + 1]);
				read_jobs[i].dst  = gpu_upload_area;
				read_jobs[i].size = 256u << 20;
			}
		}

		auto waitable_files = cross::read_files(manager, read_jobs);
		waitable_files->wait();
	}

	manager.destroy();
}

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

void test_sync(int argc, char *argv[])
{
	EXO_PROFILE_SCOPE
	{
		EXO_PROFILE_SCOPE_NAMED("foreach test")
		std::vector<int> values;
		{
			EXO_PROFILE_SCOPE_NAMED("vector creating")
			values.reserve(VECTOR_SIZE);
			for (u32 i = 0; i < VECTOR_SIZE; ++i) {
				values.push_back(i32(i));
			}
		}

		auto before_wait = __rdtsc();
		for (auto &value : values) {
			EXO_PROFILE_SCOPE_NAMED("Expensive int calculation")
			double accum = 0;
			for (int i = 0; i < value; ++i) {
				accum += sqrt(double(i));
			}
			value = int(accum);
		}
		auto after_wait = __rdtsc();
		printf("waited for %llu cycles for the tasks to finish\n", after_wait - before_wait);
	}

	{
		EXO_PROFILE_SCOPE_NAMED("read files test")
		std::vector<cross::ReadFileJobDesc> read_jobs;
		void                               *gpu_buffer      = nullptr;
		std::span<u8>                       gpu_upload_area = {};
		{
			EXO_PROFILE_SCOPE_NAMED("prepare dst buffer")
			gpu_buffer      = std::malloc(256 << 20);
			gpu_upload_area = std::span<u8>((u8 *)gpu_buffer, 256 << 20);
		}
		{
			EXO_PROFILE_SCOPE_NAMED("prepare read jobs")
			read_jobs.resize(std::size_t(argc) - 1);
			for (std::size_t i = 0, end = std::size_t(argc) - 1; i < end; ++i) {
				read_jobs[i].path = std::string_view(argv[i + 1]);
				read_jobs[i].dst  = gpu_upload_area;
				read_jobs[i].size = 256u << 20;
			}
		}

		for (const auto &job_desc : read_jobs) {
			HANDLE file_handle = NULL;
			{
				EXO_PROFILE_SCOPE_NAMED("Open file")
				auto filepath = utils::utf8_to_utf16(job_desc.path);
				file_handle   = CreateFile(filepath.c_str(),
                    GENERIC_READ,
                    0,
                    NULL,
                    OPEN_EXISTING,
                    FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING,
                    NULL);

				auto last_error = GetLastError();

				ASSERT(file_handle != INVALID_HANDLE_VALUE);
				ASSERT(last_error != ERROR_IO_PENDING);
			}
			{

				EXO_PROFILE_SCOPE_NAMED("Read file")
				OVERLAPPED ovl = {};
				ovl.Offset     = DWORD(job_desc.offset);
				ovl.OffsetHigh = DWORD(job_desc.offset >> 32);
				auto res       = ReadFile(file_handle, job_desc.dst.data(), DWORD(job_desc.size), NULL, &ovl);

				auto last_error = GetLastError();
				ASSERT(res && last_error != ERROR_IO_PENDING);
			}
		}
	}
}

void test_std()
{
	EXO_PROFILE_SCOPE
	{
		EXO_PROFILE_SCOPE_NAMED("foreach test")
		std::vector<int> values;
		{
			EXO_PROFILE_SCOPE_NAMED("vector creating")
			values.reserve(VECTOR_SIZE);
			for (u32 i = 0; i < VECTOR_SIZE; ++i) {
				values.push_back(i32(i));
			}
		}

		auto values_span = std::span(values);

		auto before_wait = __rdtsc();
		std::for_each(std::execution::par, std::begin(values_span), std::end(values_span), [](int &value) {
			EXO_PROFILE_SCOPE_NAMED("Expensive int calculation")
			double accum = 0;
			for (int i = 0; i < value; ++i) {
				accum += sqrt(double(i));
			}
			value = int(accum);
		});
		auto after_wait = __rdtsc();
		printf("waited for %llu cycles for the tasks to finish\n", after_wait - before_wait);
	}
}

int main(int argc, char *argv[])
{
	EXO_PROFILE_SCOPE

	test_async(argc, argv);
	test_sync(argc, argv);
	test_std();

	EXO_PROFILE_FRAMEMARK;
}
