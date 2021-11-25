#pragma once
#include "exo/maths/numerics.h"
#include "exo/collections/index_map.h"
#include <string_view>

/**
   A StringRepository is a string interner, it contains immutable strings.
   Strings interned in a repository can be compared using their pointers instead of memcmp, and all interned strings
sharing the same value will point to the same pointer.

   Individual strings CAN NOT be freed from the repository, but the entire repository can be freed at once.

   Reference: https://ourmachinery.com/post/data-structures-part-3-arrays-of-arrays/
**/

struct StringRepository
{
    StringRepository(usize _capacity = 1 << 20);
    ~StringRepository();

    StringRepository(const StringRepository &other) = delete;
    StringRepository &operator=(const StringRepository &other) = delete;

    StringRepository(StringRepository &&other) = delete;
    StringRepository &operator=(StringRepository &&other) = delete;

    u64         intern(std::string_view s);
    bool        is_interned(std::string_view s);
    const char *get_str(u64 offset);

  private:
    IndexMap offsets       = {};
    char    *string_buffer = nullptr;
    usize    buffer_size   = 0;
    usize    capacity      = 0;
};
