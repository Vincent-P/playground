#pragma once
#include <chrono>

using Clock = std::chrono::high_resolution_clock;
using TimePoint = std::chrono::time_point<Clock>;

template <typename T>
inline T elapsed_ms(TimePoint start, TimePoint end)
{
    return std::chrono::duration<T, std::milli>(end-start).count();
}
