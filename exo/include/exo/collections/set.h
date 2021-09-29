#pragma once
#include <parallel_hashmap/phmap.h>

template<typename T>
using Set = phmap::flat_hash_set<T>;
