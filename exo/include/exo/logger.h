#pragma once

#include <fmt/core.h>
#include <fmt/color.h>
#include <fmt/ostream.h>

namespace logger
{
    template <typename S, typename... Args>
    inline void info(const S& format_str, Args&&... args)
    {
        fmt::print(format_str, args...);
    }

    template <typename S, typename... Args>
    inline void error(const S& format_str, Args&&... args)
    {
        #if 0
        auto style = fg(fmt::color::crimson) | fmt::emphasis::bold;
        fmt::print(stderr, style, format_str, args...);
        #endif
        fmt::print(stderr, format_str, args...);
    }

}
