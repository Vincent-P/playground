#pragma once
#include "exo/maths/numerics.h"

namespace exo
{

struct LinearAllocator
{
  public:
    static LinearAllocator with_external_memory(void *p, usize len);

    LinearAllocator()                             = default;
    LinearAllocator(const LinearAllocator &other) = delete;
    LinearAllocator &operator=(const LinearAllocator &other) = delete;
    LinearAllocator(LinearAllocator &&other);
    LinearAllocator &operator=(LinearAllocator &&other);

    void *allocate(usize size);
    void  rewind(void *p);

    void *get_ptr() const
    {
        return ptr;
    }

  private:
    u8 *ptr = nullptr;
    u8 *end = nullptr;
};

inline u8                tls_data[16 << 20];
inline thread_local auto tls_allocator = LinearAllocator::with_external_memory(tls_data, sizeof(tls_data));

}; // namespace exo
