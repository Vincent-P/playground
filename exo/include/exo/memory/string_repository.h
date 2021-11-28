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
    StringRepository();
    ~StringRepository();

    StringRepository(const StringRepository &other) = delete;
    StringRepository &operator=(const StringRepository &other) = delete;

    StringRepository(StringRepository &&other);
    StringRepository &operator=(StringRepository &&other);

    const char *intern(std::string_view s);
    bool        is_interned(std::string_view s);

  private:
    IndexMap offsets       = {};
    char    *string_buffer = nullptr;
    usize    buffer_size   = 0;
};
