#pragma once
#include <chrono>

using Clock = std::chrono::high_resolution_clock;
using Timepoint = std::chrono::time_point<Clock>;

template <typename T>
inline T elapsed_ms(Timepoint start, Timepoint end)
{
    return std::chrono::duration<T, std::milli>(end-start).count();
}
