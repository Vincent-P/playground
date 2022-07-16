#pragma once

#include <optional>

namespace exo
{
template <typename T> using Option = std::optional<T>;

template <typename T, typename... Args> exo::Option<T> Some(Args &&...args)
{
	return std::make_optional<T>(std::forward<Args>(args)...);
}

template <typename T> exo::Option<T> Some(T &&t) { return exo::Option<T>(std::forward<T>(t)); }

inline constexpr auto None = std::nullopt;
} // namespace exo

using exo::None;
using exo::Option;
using exo::Some;
