#pragma once
#include <fmt/color.h>
#include <fmt/core.h>
#include <fmt/ostream.h>

namespace exo::logger
{
template <typename S, typename... Args> inline void info(const S &format_str, Args &&...args)
{
	fmt::print(format_str, args...);
}

template <typename S, typename... Args> inline void error(const S &format_str, Args &&...args)
{
	auto style = fg(fmt::color::crimson) | fmt::emphasis::bold;
	fmt::print(stderr, style, format_str, args...);
}

} // namespace exo::logger
