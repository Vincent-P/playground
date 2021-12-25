#pragma once
#include <vector>

namespace exo
{
template <typename T>
using Vec = std::vector<T>;

template<typename T>
inline T &emplace_back(Vec<T> &vector)
{
    vector.emplace_back();
    return vector.back();
}
}
