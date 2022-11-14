#include "utils_win32.h"

#include "exo/macros/assert.h"
#include "exo/maths/numerics.h"
#include <windows.h>

namespace cross::utils
{
std::wstring utf8_to_utf16(const exo::StringView &str)
{
	if (str.empty()) {
		return {};
	}

	const int res = MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), nullptr, 0);
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

exo::String utf16_to_utf8(const std::wstring_view &wstr)
{
	if (wstr.empty()) {
		return {};
	}

	const int res = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
	ASSERT(res > 0);
	auto        size_needed = static_cast<usize>(res);
	exo::String result;
	result.resize(size_needed);
	WideCharToMultiByte(CP_UTF8,
		0,
		wstr.data(),
		static_cast<int>(wstr.size()),
		result.data(),
		static_cast<int>(result.size()),
		nullptr,
		nullptr);
	return result;
}

bool is_handle_valid(HANDLE handle) { return handle != nullptr && handle != INVALID_HANDLE_VALUE; }
} // namespace cross::utils
