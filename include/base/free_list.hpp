#pragma once
#include "base/types.hpp"

class FreeList
{
public:
    static FreeList create(u32 capacity);

    u32 allocate();
    void free(u32 index);

    void destroy();
private:
    u32 *array = nullptr;
    u32 head = 0;
    u32 capacity;
};
