#include "exo/memory/scope_stack.h"
#include "exo/string.h"
#include "exo/string_view.h"
#include <fmt/core.h>
#include <utility>

namespace exo
{
template <typename... Args>
StringView format(ScopeStack &scope, const char *fmt, Args... args)
{
	auto  size = fmt::formatted_size(fmt, std::forward<Args>(args)...);
	char *ptr  = static_cast<char *>(scope.allocate(size + 1));
	fmt::format_to(ptr, fmt, std::forward<Args>(args)...);
	ptr[size] = '\0';
	return StringView{ptr, size};
}

template <typename... Args>
String format(const char *fmt, Args... args)
{
	String result;
	auto   size = fmt::formatted_size(fmt, std::forward<Args>(args)...);
	result.resize(size);
	fmt::format_to(result.data(), fmt, std::forward<Args>(args)...);
	return result;
}
}; // namespace exo

template <>
struct fmt::formatter<exo::StringView>
{
	constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) { return ctx.begin() ; }

	template <typename FormatContext>
	auto format(const exo::StringView &view, FormatContext &ctx) const -> decltype(ctx.out())
	{
		return fmt::format_to(ctx.out(), "{:.{}}", view.data(), view.size());
	}
};

template <>
struct fmt::formatter<exo::String>
{
	constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) { return ctx.begin() ; }

	template <typename FormatContext>
	auto format(const exo::String &str, FormatContext &ctx) const -> decltype(ctx.out())
	{
		return fmt::format_to(ctx.out(), "{:.{}}", str.data(), str.size());
	}
};
