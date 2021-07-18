#pragma once
#include "exo/types.h"
#include "exo/hash.h"

template <typename T> class Pool;

/// --- Handle type (Typed index that can be invalid)
template <typename T> struct Handle
{
    static Handle invalid()
    {
        return Handle();
    }

    Handle()
        : index(u32_invalid)
        , gen(u32_invalid)
    {
    }

    // The gen should be incremented only when creating explicitly a new handle
    explicit Handle(u32 i)
        : index(i)
    {
        static u32 cur_gen = 0;
        gen                = cur_gen++;

        assert(index != u32_invalid);
    }

    constexpr Handle &operator=(const Handle &other) = default;

    [[nodiscard]] u32 value() const { return index; }

    [[nodiscard]] u64 hash() const
    {
        return u64(index) << 32 | gen;
    }

    [[nodiscard]] bool is_valid() const
    {
        return index != u32_invalid && gen != u32_invalid;
    }

    bool operator==(const Handle &b) const
    {
        return index == b.index && gen == b.gen;
    }

    bool operator<(const Handle &b) const
    {
        return index < b.index;
    }

  private:
    u32 index;
    u32 gen;

    friend struct ::std::hash<Handle<T>>;
    friend Pool<T>;
};

namespace std
{
    template<typename T>
    struct hash<Handle<T>>
    {
        std::size_t operator()(Handle<T> const& handle) const noexcept
        {
            usize hash = hash_value(handle.index);
            hash_combine(hash, handle.gen);
            return hash;
        }
    };
}
