#pragma once
#include <phmap.h>

namespace exo
{

template <typename K, typename V>
using Map = phmap::flat_hash_map<K, V>;

}
