#pragma once
#include <exo/maths/numerics.h>

namespace vulkan
{
    enum struct QueueType : u8
    {
        Graphics = 0,
        Compute  = 1,
        Transfer = 2,
        Count
    };
}
