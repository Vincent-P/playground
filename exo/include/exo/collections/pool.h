#pragma once

#include "exo/prelude.h"
#include "exo/handle.h"
#include "exo/collections/iterator_facade.h"
#include <iterator>
#include <xutility>

/**
   A Pool is a linear allocator with a free-list.
   Performance:
     Adding/removing elements is O(1).
     Iterating is O(capacity) and elements are NOT tighly packed because of the free-list.

   TODO: Add run length of holes in the metdata to skip big holes when iterating.
 **/

// Impl below helpers
template<typename T>
struct PoolIterator;

template <typename T>
struct Pool
{
    static constexpr u32 ELEMENT_SIZE = sizeof(T) < sizeof(u32) ? sizeof(u32) : sizeof(T);

    Pool() = default;
    Pool(u32 capacity);

    Handle<T> add(T &&value);
    const T *get(Handle<T> handle) const;
    T *get(Handle<T> handle);
    void remove(Handle<T> handle);


    static_assert(std::forward_iterator<PoolIterator<T>>);
    PoolIterator<T> begin();
    PoolIterator<T> end();

    bool operator==(const Pool &rhs) const = default;

    void *buffer   = {};
    u32 freelist_head = u32_invalid;
    u32 size     = {};
    u32 capacity = {};
};

// Helpers
namespace
{
    // Each element in the buffer is prepended with metadata
    union ElementMetadata
    {
        struct
        {
            u32 is_occupied : 1;
            u32 generation : 31;
        };
        u32 raw;
    };

    template<typename T>
    T* element_ptr(Pool<T> &pool, u32 i)
    {
        return reinterpret_cast<T*>(ptr_offset(pool.buffer, i * (Pool<T>::ELEMENT_SIZE + sizeof(ElementMetadata)) + sizeof(ElementMetadata)));
    }

    template<typename T>
    const T* element_ptr(const Pool<T> &pool, u32 i)
    {
        return reinterpret_cast<const T*>(ptr_offset(pool.buffer, i * (Pool<T>::ELEMENT_SIZE + sizeof(ElementMetadata)) + sizeof(ElementMetadata)));
    }

    template<typename T>
    u32* freelist_ptr(Pool<T> &pool, u32 i)
    {
        return reinterpret_cast<u32*>(element_ptr(pool, i));
    }

    template<typename T>
    const u32* freelist_ptr(const Pool<T> &pool, u32 i)
    {
        return reinterpret_cast<const u32*>(element_ptr(pool, i));
    }

    template<typename T>
    ElementMetadata* metadata_ptr(Pool<T> &pool, u32 i)
    {
        return reinterpret_cast<ElementMetadata*>(ptr_offset(pool.buffer, i * (Pool<T>::ELEMENT_SIZE + sizeof(ElementMetadata))));
    }

    template<typename T>
    const ElementMetadata* metadata_ptr(const Pool<T> &pool, u32 i)
    {
        return reinterpret_cast<const ElementMetadata*>(ptr_offset(pool.buffer, i * (Pool<T>::ELEMENT_SIZE + sizeof(ElementMetadata))));
    }
}

template<typename T>
struct PoolIterator : IteratorFacade<PoolIterator<T>>
{
    PoolIterator() = default;
    PoolIterator(Pool<T> *_pool, u32 _index = 0)
        : pool{_pool}
        , current_index{_index}
    {
    }

    std::pair<Handle<T>, T*> dereference() const
    {
        auto *metadata = metadata_ptr(*pool, current_index);
        auto *element  = element_ptr(*pool, current_index);

        Handle<T> handle = {current_index, metadata->generation};
        return std::make_pair(handle, element);
    }

    void increment()
    {
        for (current_index = current_index + 1; current_index < pool->capacity; current_index += 1)
        {
            auto *metadata = metadata_ptr(*pool, current_index);
            auto *element  = element_ptr(*pool, current_index);
            if (metadata->is_occupied == 1)
            {
                break;
            }
        }
    }

    bool equal_to(const PoolIterator &other) const
    {
        return pool == other.pool && current_index == other.current_index;
    }

    Pool<T> *pool        = nullptr;
    u32 current_index = u32_invalid;
};


template<typename T>
Pool<T>::Pool(u32 capacity)
{
    buffer = malloc(capacity * (Pool<T>::ELEMENT_SIZE + sizeof(ElementMetadata)));

    // Init the free list
    freelist_head = 0;
    for (u32 i = 0; i < capacity - 1; i += 1)
    {
        auto *metadata = metadata_ptr(*this, i) ;
        metadata->raw = 0;

        u32 *freelist_element = freelist_ptr(*this, i);
        *freelist_element = i + 1;
    }
    *freelist_ptr(*this, capacity-1) = u32_invalid;
}

template <typename T>
Handle<T> Pool<T>::add(T &&value)
{
    // Realloc memory if the container is full
    if (freelist_head == u32_invalid)
    {
        ASSERT(size + 1 >= capacity);
        auto new_capacity = capacity > 0 ? capacity * 2 : 64;
        void *new_buffer = realloc(buffer, new_capacity * (Pool<T>::ELEMENT_SIZE + sizeof(ElementMetadata)));
        ASSERT(new_buffer != nullptr);
        buffer   = new_buffer;

        // extend the freelist
        freelist_head = capacity;
        for (u32 i = capacity; i < new_capacity - 1; i += 1)
        {
            auto *metadata = metadata_ptr(*this, i) ;
            metadata->raw = 0;

            u32 *freelist_element = freelist_ptr(*this, i);
            *freelist_element = i + 1;
        }
        *freelist_ptr(*this, new_capacity - 1) = u32_invalid;

        capacity = new_capacity;
    }

    // Pop the free list head to find an empty place for the new element
    u32 i_element = freelist_head;
    freelist_head = *freelist_ptr(*this, i_element);

    // Construct the new element
    T *   element  = new (element_ptr(*this, i_element)) T{std::forward<T>(value)};
    auto *metadata = metadata_ptr(*this, i_element);
    ASSERT(metadata->is_occupied == 0);
    metadata->is_occupied = 1;

    size += 1;

    return {i_element, metadata->generation};
}

template <typename T>
const T *Pool<T>::get(Handle<T> handle) const
{
    u32   i_element = handle.index;
    auto *metadata  = metadata_ptr(*this, i_element);
    auto *element   = element_ptr(*this, i_element);

    return handle.index < size && metadata->generation == handle.gen ? element : nullptr;
}

template <typename T>
T *Pool<T>::get(Handle<T> handle)
{
    return const_cast<T *>(static_cast<const Pool<T> &>(*this).get(handle));
}

template <typename T>
void Pool<T>::remove(Handle<T> handle)
{
    auto *metadata = metadata_ptr(*this, handle.index);
    auto *element    = element_ptr(*this, handle.index);
    auto *freelist   = freelist_ptr(*this, handle.index);

    ASSERT(handle.index < size);
    ASSERT(metadata->generation == handle.gen);
    ASSERT(metadata->is_occupied == 1);

    // Destruct the element
    element->~T();
    metadata->generation = metadata->generation + 1;
    metadata->is_occupied = 0;

    // Push this slot to the head of the free list
    *freelist = freelist_head;
    freelist_head = handle.index;

    size = size - 1;
}

template <typename T>
PoolIterator<T> Pool<T>::begin()
{
    return PoolIterator<T>(this, 0);
}

template <typename T>
PoolIterator<T> Pool<T>::end()
{
    return PoolIterator<T>(this, capacity);
}
