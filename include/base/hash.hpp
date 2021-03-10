#pragma once
#include <functional>

template <typename T>
inline std::size_t hash_value(const T& v)
{
    return std::hash<T>{}(v);
}

template <typename T>
inline void hash_combine(std::size_t& seed, const T& v)
{
    seed ^= hash_value(v) + 0x9e3779b9 + (seed<<6) + (seed>>2);
}

namespace std
{
    template<typename T>
    struct hash<std::vector<T>>
    {
        std::size_t operator()(std::vector<T> const& vec) const noexcept
        {
            std::size_t hash = vec.size();
            for (auto &i : vec)
            {
                hash_combine(hash, i);
            }
            return hash;
        }
    };
}
