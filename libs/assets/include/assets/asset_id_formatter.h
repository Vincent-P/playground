#pragma once
#include "assets/asset_id.h"
#include <fmt/format.h>

template <>
struct fmt::formatter<AssetId>
{
	constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) { return ctx.begin(); }

	template <typename FormatContext>
	auto format(const AssetId &id, FormatContext &ctx) -> decltype(ctx.out())
	{
		if (id.is_valid()) {
			return format_to(ctx.out(), "{}", id.name.c_str());
		} else {
			return format_to(ctx.out(), "INVALID");
		}
	}
};
