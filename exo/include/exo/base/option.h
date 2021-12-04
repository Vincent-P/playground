#pragma once

#include <optional>

template<typename T>
using Option = std::optional<T>;

template<typename T, typename ...Args>
Option<T> Some(Args &&... args)
{
    return std::make_optional<T>(std::forward<Args>(args)...);
}

template<typename T>
Option<T> Some(T && t)
{
    return Option<T>(std::forward<T>(t));
}

inline constexpr auto None = std::nullopt;
