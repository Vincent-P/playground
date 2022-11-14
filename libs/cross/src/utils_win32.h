#pragma once
#include "exo/string.h"
#include "exo/string_view.h"
#include <string>

using HANDLE = void *;
namespace cross::utils
{
std::wstring utf8_to_utf16(const exo::StringView &str);
exo::String  utf16_to_utf8(const std::wstring_view &wstr);
bool         is_handle_valid(HANDLE handle);
}; // namespace cross::utils
