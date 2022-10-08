#pragma once

// clang 15.0.2 does not define __cpp_consteval on Windows
#ifndef __cpp_consteval
#define __cpp_consteval
#endif

#include <source_location>

void internal_assert_trigger(const char *condition_str, const std::source_location location);

#define ASSERT(expr)                                                                                                   \
	if (!(expr)) [[unlikely]] {                                                                                        \
		internal_assert_trigger(#expr, std::source_location::current());                                               \
	}
