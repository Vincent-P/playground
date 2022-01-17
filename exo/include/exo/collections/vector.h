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

template<typename T>
inline void vector_insert_unique(Vec<T> &vector, const T &element)
{
    for (const auto &value : vector)
    {
        if (value == element)
        {
            return;
        }
    }
    vector.push_back(element);
}
}
