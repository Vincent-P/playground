#pragma once
#include "exo/maths/numerics.h"
#include "exo/collections/vector.h"
#include "exo/option.h"

/**
   A hash map using linear probing.
   The keys are 64-bit hash and the values are indices into an external array.
   This allows to make a non-templated hashmap that works with any type.
 **/

namespace exo
{
struct IndexMap
{
    static IndexMap with_capacity(u64 c);
    ~IndexMap();

    // Move-only struct
    IndexMap()                      = default;
    IndexMap(const IndexMap &other) = delete;
    IndexMap &operator=(const IndexMap &other) = delete;
    IndexMap(IndexMap &&other);
    IndexMap &operator=(IndexMap &&other);

    Option<u64> at(u64 hash);
    void        insert(u64 hash, u64 index);
    void        remove(u64 hash);

  private:
    void check_growth();

    u64  *keys     = nullptr;
    u64  *values   = nullptr;
    usize capacity = 0;
    usize size     = 0;
};
} // namespace exo
