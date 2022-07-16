#pragma once
#include "exo/uuid.h"
#include <fmt/format.h>

template <> struct fmt::formatter<exo::UUID>
{
	constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) { return ctx.begin(); }

	template <typename FormatContext> auto format(const exo::UUID &uuid, FormatContext &ctx) -> decltype(ctx.out())
	{
		return format_to(ctx.out(), "{:.{}}", uuid.str, exo::UUID::STR_LEN);
	}
};
