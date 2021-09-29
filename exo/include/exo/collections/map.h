#pragma once
#include <parallel_hashmap/phmap.h>

template<typename K, typename V>
using Map = phmap::flat_hash_map<K, V>;
