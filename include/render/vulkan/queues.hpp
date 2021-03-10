#pragma once
#include "base/types.hpp"

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
