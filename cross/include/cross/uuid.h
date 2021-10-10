#pragma once

#include <exo/maths/numerics.h>
#include "cross/prelude.h"

#include <exo/collections/map.h>

namespace cross
{
    struct UUID
    {
        u32 data[4];

        static constexpr usize STR_LEN = 35;

        char str[STR_LEN];

        static UUID create();
        bool operator==(const UUID &other) const = default;

        friend size_t hash_value(const UUID &uuid);
    };
}

static_assert(phmap::has_hash_value<cross::UUID>::value);
