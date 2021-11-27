#pragma once
#include "exo/maths/numerics.h"
#include "exo/collections/vector.h"
#include "exo/base/option.h"

/**
   A hash map using linear probing.
   The keys are 64-bit hash and the values are indices into an external array.
   This allows to make a non-templated hashmap that works with any type.
 **/

struct IndexMap
{
    IndexMap(u64 _capacity = 32);
    ~IndexMap();

    Option<u64> at(u64 hash);
    void        insert(u64 hash, u64 index);
    void        remove(u64 hash);

  private:
    void check_growth();

    u64  *keys;
    u64  *values;
    usize capacity;
    usize size;
};
