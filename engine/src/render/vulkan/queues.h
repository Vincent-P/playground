#pragma once
#include <exo/types.h>

namespace vulkan
{
    enum struct QueueType : u8
    {
        Graphics = 0,
        Compute  = 1,
        Transfer = 2,
        None
    };
}
