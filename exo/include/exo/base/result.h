#pragma once
#include <leaf.hpp>

namespace leaf = boost::leaf;

template<typename T>
using Result = boost::leaf::result<T>;

template<typename T, typename ...Args>
Result<T> Ok(Args &&... args)
{
    return Result<T>(std::forward<Args>(args)...);
}

template<typename T>
Result<T> Ok(T && t)
{
    return Result<T>(std::forward<T>(t));
}

template<typename ...Args>
leaf::error_id Err(Args &&... args)
{
    return leaf::new_error(std::forward<Args>(args)...);
}
