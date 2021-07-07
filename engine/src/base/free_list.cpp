#include "base/free_list.hpp"
#include <cstdlib>

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
    return list;
}

u32 FreeList::allocate()
{
    u32 free_index = this->head;
    assert(free_index != u32_invalid);
    assert(head < this->capacity);
    this->head = this->array[head];
    return free_index;
}

void FreeList::free(u32 index)
{
    assert(index < this->capacity);
    this->array[index] = this->head;
    this->head = index;
}

void FreeList::destroy()
{
    std::free(this->array);
}
