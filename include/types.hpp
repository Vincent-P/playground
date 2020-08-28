#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>
#include <variant>
#include <vector>
#include <chrono>

#define NO_COPY_NO_MOVE(name)                                                                                          \
    name(const name &other)  = delete;                                                                                 \
    name(const name &&other) = delete;                                                                                 \
    name operator=(const name &other) = delete;                                                                        \
    name operator=(const name &&other) = delete;

#define ARRAY_SIZE(_arr) (sizeof(_arr) / sizeof(*_arr))

#define MEMBER_OFFSET(type, member) (static_cast<u32>(reinterpret_cast<u64>(&reinterpret_cast<type *>(0)->member)))

#define not_implemented()                                                                                              \
    {                                                                                                                  \
        assert(false);                                                                                                 \
    }

#define PACKED __attribute__((packed))

#if defined(_WIN64)
#define PRINT_u64 "%llu"
#else
#define PRINT_u64 "%lu"
#endif

constexpr float PI = 3.1415926535897932384626433832795f;

/// --- Numeric Types
using i8    = std::int8_t;
using i16   = std::int16_t;
using i32   = std::int32_t;
using i64   = std::int64_t;
using u8    = std::uint8_t;
using u16   = std::uint16_t;
using u32   = std::uint32_t;
using u64   = std::uint64_t;
using usize = std::size_t;
using uchar = unsigned char;
using uint  = unsigned int;

static constexpr u32 u32_invalid = ~0u;

/// --- Vector types
using float2   = glm::vec2;
using float3   = glm::vec3;
using float4   = glm::vec4;
using int2     = glm::ivec2;
using int3     = glm::ivec3;
using int4     = glm::ivec4;
using uint2     = glm::uvec2;
using uint3     = glm::uvec3;
using uint4     = glm::uvec4;
using float4x4 = glm::mat4;


namespace my_app
{
// --- User-defined literals

inline uint operator"" _K(unsigned long long value)
{
    return value * 1000u;
}

inline uint operator"" _KiB(unsigned long long value)
{
    return value * 1024u;
}

inline uint operator"" _MiB(unsigned long long value)
{
    return value * 1024u * 1024u;
}


inline uint operator"" _GiB(unsigned long long value)
{
    return value * 1024u * 1024u * 1024;
}

/// --- Utility functions
template <typename T> inline T *ptr_offset(T *ptr, usize offset)
{
    return reinterpret_cast<T *>(reinterpret_cast<char *>(ptr) + offset);
}

template <typename vector_source, typename vector_dest, typename transform_function>
inline void map_transform(const vector_source &src, vector_dest &dst, transform_function f)
{
    dst.reserve(src.size());
    std::transform(src.begin(), src.end(), std::back_inserter(dst), f);
}

inline usize round_up_to_alignment(usize alignment, usize bytes)
{
    const usize mask = alignment - 1;
    return (bytes + mask) & ~mask;
}

/// --- Handle type (Typed index that can be invalid)
template <typename T> struct Handle
{
    static Handle invalid()
    {
        return Handle(u32_invalid);
    }
    Handle()
        : index(u32_invalid)
    {
    }

    explicit Handle(u32 i)
        : index(i)
    {
        assert(index != u32_invalid);
        static u32 cur_gen = 0;
        gen                = cur_gen++;
    }

    [[nodiscard]] u32 value() const
    {
        return index;
    }
    [[nodiscard]] bool is_valid() const
    {
        return index != u32_invalid;
    }

    bool operator==(const Handle &b) const = default;

  private:
    u32 index;
    u32 gen;

    friend struct ::std::hash<Handle<T>>;
};
}

namespace std
{
    template<typename T>
    struct hash<my_app::Handle<T>>
    {
        std::size_t operator()(my_app::Handle<T> const& handle) const noexcept
        {
            std::size_t h1 = std::hash<u32>{}(handle.index);
            std::size_t h2 = std::hash<u32>{}(handle.gen);
            return h1 ^ (h2 << 1); // or use boost::hash_combine
        }
    };
}

namespace my_app
{
/// --- Pool allocator
template <typename T> class Pool
{
    class Iterator
    {
      public:
        using difference_type   = int;
        using value_type        = std::pair<Handle<T>, T*>;
        using pointer           = value_type *;
        using reference         = value_type &;
        using iterator_category = std::input_iterator_tag;

        Iterator() = default;

        Iterator(Pool &_pool, usize _index = 0)
            : pool{&_pool}
            , current_index{_index}
            , value{}
        {
            for (; current_index < pool->data.size(); current_index++)
            {
                // index() returns a zero-based index of the type
                // 0: handle_type
                // 1: T
                if (pool->data[current_index].index() == 1)
                {
                    break;
                }
            }
        }

        Iterator(const Iterator &rhs)
        {
            this->pool         = rhs.pool;
            this->current_index = rhs.current_index;
        }

        Iterator(Iterator &&rhs)
        {
            this->pool         = rhs.pool;
            this->current_index = rhs.current_index;
        }

        Iterator &operator=(const Iterator &rhs)
        {
            this->pool          = rhs.pool;
            this->current_index = rhs.current_index;
            return *this;
        }

        Iterator &operator=(Iterator &&rhs)
        {
            this->pool         = rhs.pool;
            this->current_index = rhs.current_index;
            return *this;
        }

        bool operator==(const Iterator &rhs) const
        {
            return current_index == rhs.current_index;
        }

        reference operator*()
        {
            assert(this->pool && current_index < this->pool->get_size());
            value = std::make_pair(this->pool->keys[current_index], &this->pool->get_value_internal(current_index));
            return value;
        }

        Iterator &operator++()
        {
            assert(this->pool);
            current_index++;
            for (; current_index < pool->data.size(); current_index++)
            {
                // index() returns a zero-based index of the type
                // 0: handle_type
                // 1: T
                if (pool->data[current_index].index() == 1)
                {
                    break;
                }
            }
            return *this;
        }

        Iterator &operator++(int n)
        {
            assert(this->pool && n > 0);
            for (int i = 0; i < n; i++)
            {
                for (; current_index < pool->data.size(); current_index++)
                {
                    // index() returns a zero-based index of the type
                    // 0: handle_type
                    // 1: T
                    if (pool->data[current_index].index() == 1)
                    {
                        break;
                    }
                }
            }
            return *this;
        }
    private:
        Pool *pool        = nullptr;
        usize current_index = 0;
        value_type value = {};
    };

    // static_assert(std::input_iterator<Iterator>);

    using handle_type  = Handle<T>;
    using element_type = std::variant<handle_type, T>; // < next_free_ptr, value >

    handle_type &get_handle_internal(handle_type handle)
    {
        return std::get<handle_type>(data[handle.value()]);
    }

    T &get_value_internal(u32 index)
    {
        return std::get<T>(data[index]);
    }

    T &get_value_internal(handle_type handle)
    {
        return get_value_internal(handle.value());
    }

  public:
    Pool() = default;
    Pool(usize capacity)
    {
        data.reserve(capacity);
        keys.reserve(capacity);
    }

    handle_type add(T &&value)
    {
        size += 1;

        if (!first_free.is_valid())
        {
            data.push_back(std::move(value));
            auto handle = handle_type(data.size() - 1);
            keys.push_back(handle);
            return handle;
        }

        // Pop the free list
        handle_type old_first_free = first_free;
        first_free                 = get_handle_internal(old_first_free);

        // put the value in
        data[old_first_free.value()] = std::move(value);
        keys[old_first_free.value()] = old_first_free;

        return old_first_free;
    }

    T *get(handle_type handle)
    {
        if (!handle.is_valid())
        {
            assert(!"invalid handle");
            return nullptr;
        }

        if (handle != keys[handle.value()])
        {
            assert(!"use after free");
            return nullptr;
        }

        return &get_value_internal(handle);
    }

    void remove(handle_type handle)
    {
        assert(size != 0);
        if (!handle.is_valid())
        {
            assert(false);
        }

        size -= 1;

        // replace the value to remove with the head of the free list
        auto &data_element = data[handle.value()];
        auto &key_element  = keys[handle.value()];
        data_element       = first_free;
        key_element        = handle_type::invalid();

        // set the new head of the free list to the handle that was just removed
        first_free    = handle;
    }

    Iterator begin()
    {
        return Iterator(*this, 0);
    }

    Iterator end()
    {
        return Iterator(*this, data.size());
    }

    bool operator==(const Pool &rhs) const = default;

    usize get_size() const
    {
        return size;
    }

  private:
    handle_type first_free; // free list head ptr
    std::vector<element_type> data;
    std::vector<handle_type> keys;
    usize size{0};

    friend class Iterator;
};

/// Clock
using Clock = std::chrono::high_resolution_clock;
using TimePoint = std::chrono::time_point<Clock>;

template <typename T>
inline T elapsed_ms(TimePoint start, TimePoint end)
{
    return std::chrono::duration<T, std::milli>(end-start).count();
}

/// Fat pointer

struct FatPtr
{
    void* data{nullptr};
    usize size{0};

    bool operator==(const FatPtr&) const = default;
};

} // namespace my_app
