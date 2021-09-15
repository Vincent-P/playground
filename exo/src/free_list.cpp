#include "exo/free_list.h"

#include <cstdlib>
#include <cassert>

FreeList FreeList::create(u32 capacity)
{
    FreeList list;
    list.array = reinterpret_cast<u32*>(std::malloc(capacity * sizeof(u32)));

    for (u32 i = 0; i < capacity; i += 1)
    {
        list.array[i] = i + 1;
    }
    list.array[capacity-1] = u32_invalid;

    list.head = 0;
    list.capacity = capacity;
    list.size = 0;
    return list;
}

u32 FreeList::allocate()
{
    u32 free_index = this->head;
    ASSERT(free_index != u32_invalid);
    ASSERT(head < this->capacity);
    this->head = this->array[head];
    this->size += 1;
    return free_index;
}

void FreeList::free(u32 index)
{
    ASSERT(index < this->capacity);
    this->array[index] = this->head;
    this->head = index;
    this->size -= 1;
}

void FreeList::destroy()
{
    std::free(this->array);
}
