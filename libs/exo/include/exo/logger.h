#pragma once
#include <cstdarg>
#include <cstdio>

namespace exo::logger
{
inline void info(const char *fmt...)
{
	va_list args;
	va_start(args, fmt);
	std::printf(fmt, args);
	va_end(args);
}

inline void error(const char *fmt...)
{
	va_list args;
	va_start(args, fmt);
	std::fprintf(stderr, fmt, args);
	va_end(args);
}

} // namespace exo::logger
