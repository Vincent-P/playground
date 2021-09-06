#include "utils_win32.h"

#include <exo/types.h>
#include <windows.h>

namespace platform
{
std::wstring utf8_to_utf16(const std::string_view &str)
{
    if (str.empty())
    {
        return {};
    }

    int res = MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), nullptr, 0);
    assert(res > 0);
    usize size_needed = static_cast<usize>(res);
    std::wstring result(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), result.data(), static_cast<int>(result.size()));
    return result;
}

std::string utf16_to_utf8(const std::wstring_view &wstr)
{
    if (wstr.empty())
    {
        return {};
    }

    int res = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
    assert(res > 0);
    usize size_needed = static_cast<usize>(res);
    std::string result(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), result.data(), static_cast<int>(result.size()), nullptr, nullptr);
    return result;
}

bool is_handle_valid(HANDLE handle)
{
    return handle != nullptr && handle != INVALID_HANDLE_VALUE;
}
}
