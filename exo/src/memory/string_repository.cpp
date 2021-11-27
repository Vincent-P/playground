#include "exo/memory/string_repository.h"

#include "exo/prelude.h"
#include "exo/cross/memory/virtual_allocator.h"

#define XXH_INLINE_ALL
#define XXH_PRIVATE_API
#include <xxhash/xxhash.h>

StringRepository::StringRepository()
{
    this->string_buffer = reinterpret_cast<char *>(virtual_allocator::reserve(1_GiB));
}

StringRepository::~StringRepository()
{
    virtual_allocator::free(this->string_buffer);
}

const char *StringRepository::intern(std::string_view s)
{
    const u64 hash = XXH3_64bits(s.data(), s.size());

    // If the string is already interned, return its offset
    if (const auto offset = offsets.at(hash))
    {
        return this->string_buffer + offset.value();
    }

    // Commit more memory if needed
    const usize page_size      = virtual_allocator::get_page_size();
    const usize old_size       = this->buffer_size;
    const usize new_size       = this->buffer_size + s.size() + 1;
    const usize page_count     = round_up_to_alignment(page_size, old_size);
    const usize new_page_count = round_up_to_alignment(page_size, new_size);
    if (new_page_count != page_count)
    {
        virtual_allocator::commit(this->string_buffer + page_count * page_size,
                                  (new_page_count - page_count) * page_size);
    }

    std::memcpy(string_buffer + this->buffer_size, s.data(), s.size() + 1);
    offsets.insert(hash, this->buffer_size);
    this->buffer_size = new_size;

    return this->string_buffer + old_size;
}

bool StringRepository::is_interned(std::string_view s)
{
    const u64 hash = XXH3_64bits(s.data(), s.size());
    return offsets.at(hash).has_value();
}
