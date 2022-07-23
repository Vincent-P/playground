#pragma once
#include <vector>

namespace exo
{
template <typename T> using Vec = std::vector<T>;

template <typename T> inline T &emplace_back(Vec<T> &vector)
{
	vector.emplace_back();
	return vector.back();
}

template <typename T> inline void vector_insert_unique(Vec<T> &vector, const T &element)
{
	for (const auto &value : vector) {
		if (value == element) {
			return;
		}
	}
	vector.push_back(element);
}

template <typename T> inline void swap_remove(Vec<T> &vector, size_t i)
{
	ASSERT(vector.size() > 0);
	if (vector.size() > 0) {
		std::swap(vector[i], vector[vector.size() - 1]);
	}
	vector.pop_back();
}
} // namespace exo

using exo::Vec;
