#pragma once
#include "exo/macros/assert.h"
#include "exo/memory/linear_allocator.h"
#include "exo/maths/pointer.h"
#include <type_traits>

namespace exo
{
struct Finalizer
{
    void (*fn)(void *ptr);
    Finalizer *chain;
};

struct ScopeStack
{
  public:
    static ScopeStack with_allocator(LinearAllocator *a);
    ~ScopeStack();

    template <typename T>
    T *allocate();

    inline void *allocate(usize size);

    ScopeStack()                        = default;
    ScopeStack(const ScopeStack &other) = delete;
    ScopeStack &operator=(const ScopeStack &other) = delete;
    ScopeStack(ScopeStack &&other);
    ScopeStack &operator=(ScopeStack &&other);

  private:
    static inline void *object_from_finalizer(Finalizer *f)
    {
        return reinterpret_cast<u8 *>(f) + round_up_to_alignment(sizeof(u32), sizeof(Finalizer));
    }

    LinearAllocator *allocator      = nullptr;
    void            *rewind_ptr     = nullptr;
    Finalizer       *finalizer_head = nullptr;
};

template <typename T>
void call_dtor(void *ptr)
{
    static_cast<T *>(ptr)->~T();
}

template <typename T>
T *ScopeStack::allocate()
{
    if constexpr (std::is_trivially_destructible_v<T>)
    {
        // Allocate enough space for the finalizer + the object itself
        const usize total_size = sizeof(T) + round_up_to_alignment(sizeof(u32), sizeof(Finalizer));
        auto       *f          = reinterpret_cast<Finalizer *>(allocator->allocate(total_size));

        // Call the constructor with placement new
        T *result = new (object_from_finalizer(f)) T;

        // Push the finalizer to the top of the list
        f->fn                = &call_dtor<T>;
        f->chain             = this->finalizer_head;
        this->finalizer_head = f;
        return result;
    }
    else
    {
        // Allocate memory for T, call its constructor
        return new (allocator->allocate(sizeof(T))) T;
    }
}

inline void *ScopeStack::allocate(usize size)
{
    return allocator->allocate(size);
}
} // namespace exo
