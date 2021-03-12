#pragma once
#include <string>
#include <string_view>

namespace platform
{
std::wstring utf8_to_utf16(const std::string_view &str);
std::string utf16_to_utf8(const std::wstring_view &wstr);
};
