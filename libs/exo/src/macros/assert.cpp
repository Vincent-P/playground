#include "exo/macros/assert.h"

#include <cstdio> // for fprintf and stderr

void internal_assert_trigger(const char *condition_str, const std::source_location location)
{
	fprintf(stderr,
		"%s:%s:%u:%u Assertion failed: %s\n",
		location.file_name(),
		location.function_name(),
		location.line(),
		location.column(),
		condition_str);

#if defined(_WIN32)
	__debugbreak();
#else
	__asm__ volatile("int $0x03");
#endif
}
