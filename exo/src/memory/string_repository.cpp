#include "exo/memory/string_repository.h"

#include "exo/prelude.h"

#define XXH_INLINE_ALL
#define XXH_PRIVATE_API
#include <xxhash/xxhash.h>

StringRepository::StringRepository(usize _capacity)
{
    this->capacity      = _capacity;
    this->string_buffer = reinterpret_cast<char *>(calloc(this->capacity, 1));
}

StringRepository::~StringRepository()
{
    free(this->string_buffer);
}

u64 StringRepository::intern(std::string_view s)
{
    u64 hash   = XXH3_64bits(s.data(), s.size());

    // If the string is already interned, return its offset
    if (auto offset = offsets.at(hash))
    {
        return offset.value();
    }

    // Otherwise, insert the current offset into the hashtable
    // and copy the string into the buffer
    u64 offset = this->buffer_size;
    offsets.insert(hash, offset);

    if (this->buffer_size + s.size() + 1 > this->capacity)
    {
        usize new_capacity = 2 * this->capacity;
        char *new_buffer = reinterpret_cast<char*>(realloc(reinterpret_cast<void*>(this->string_buffer), new_capacity));
        ASSERT(new_buffer);

        char *buffer_end = new_buffer + new_capacity;
        for (char *it = new_buffer + this->capacity; it < buffer_end; it += 1)
        {
            *it = 0;
        }
    }

    std::memcpy(string_buffer + offset, s.data(), s.size());

    // Update the current offset + 1 if the string is empty or non-null terminated to add a \0
    this->buffer_size += s.size();
    if (s.size() == 0 || s.data()[s.size() - 1] != 0)
    {
        this->buffer_size += 1;
    }

    return offset;
}

bool StringRepository::is_interned(std::string_view s)
{
    u64 hash = XXH3_64bits(s.data(), s.size());
    return offsets.at(hash).has_value();
}

const char *StringRepository::get_str(u64 offset)
{
    ASSERT(offset < this->buffer_size);
    return string_buffer + offset;
}
