#include "exo/format.h"
#include "exo/macros/assert.h"
#include "exo/memory/scope_stack.h"
#include <cstdarg>
#include <cstdio>

namespace exo
{
StringView formatf(ScopeStack &scope, const char *fmt...)
{
	va_list args1;
	va_start(args1, fmt);

	va_list args2;
	va_copy(args2, args1);

	auto formatted_size = std::vsnprintf(nullptr, 0, fmt, args1);
	ASSERT(formatted_size > 0);
	const auto buffer_size = usize(formatted_size);

	va_end(args1);

	auto *buffer        = static_cast<char *>(scope.allocate(buffer_size + 1));
	buffer[buffer_size] = '\0';
	auto result         = std::vsnprintf(buffer, buffer_size + 1, fmt, args2);
	ASSERT(result > 0);

	va_end(args2);

	return StringView{buffer, buffer_size};
}
} // namespace exo
