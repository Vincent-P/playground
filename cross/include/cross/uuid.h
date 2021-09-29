#pragma once

#include <exo/maths/numerics.h>
#include "cross/prelude.h"

namespace cross
{
    struct UUID
    {
        u32 data[4];

        static UUID create();
    };
}
