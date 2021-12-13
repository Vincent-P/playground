#pragma once
#include <phmap.h>

namespace exo
{
template <typename T>
using Set = phmap::flat_hash_set<T>;
}
