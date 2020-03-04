#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <variant>
#include <glm/glm.hpp>

#define NO_COPY_NO_MOVE(name)                                                                                          \
    name(const name &other)  = delete;                                                                                 \
    name(const name &&other) = delete;                                                                                 \
    name operator=(const name &other) = delete;                                                                        \
    name operator=(const name &&other) = delete;

#define VK_CHECK(x)                                                                                                    \
    do {                                                                                                               \
	VkResult err = x;                                                                                              \
	if (err) {                                                                                                     \
	    std::string error("Vulkan error");                                                                         \
	    error = std::to_string(err) + std::string(".");                                                            \
	    std::cerr << error << std::endl;                                                                           \
	    throw std::runtime_error(error);                                                                           \
	}                                                                                                              \
    } while (0)

#define ARRAY_SIZE(_arr) (sizeof(_arr) / sizeof(*_arr))

#define MEMBER_OFFSET(type, member) (static_cast<u32>(reinterpret_cast<u64>(&reinterpret_cast<type *>(0)->member)))

namespace my_app
{
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
using float4x4 = glm::mat4;

/// --- Utility functions

template <typename T> inline T *ptr_offset(T *ptr, usize offset) { return reinterpret_cast<T*>(reinterpret_cast<char *>(ptr) + offset); }

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
template <typename T>
struct Handle
{
    static Handle invalid() { return Handle(u32_invalid); }
    Handle() : index(u32_invalid) {}
    explicit Handle(u32 i) : index(i) {}

    [[nodiscard]] u32 value() const { return index; }
    [[nodiscard]] bool is_valid() const { return *this != invalid(); }

    friend bool operator==(Handle a, Handle b) { return a.index == b.index; }
    friend bool operator!=(Handle a, Handle b) { return a.index != b.index; }

  private:
    u32 index;
};

/// --- Arena allocator
// TODO: iterator type to be able to use ranged-based for loops, the iterator should only return values not handles...
template <typename T>
class Arena
{
    class Iterator
    {
        using difference_type = void;
        using value_type = T;
        using pointer = T*;
        using reference = T&;
        using iterator_category = std::input_iterator_tag;

    public:
      Iterator(Arena &_arena, usize _index = 0)
          : arena{_arena}
          , current_index{_index}
      {}

      value_type operator*() const { return std::get<value_type>(arena.data[current_index]); }

      Iterator &operator++() {}

      Iterator operator++(int n) {}

    private:
      Arena &arena;
      usize current_index;
    };

    using handle_type = Handle<T>;
    using element_type = std::variant<handle_type, T>;

    handle_type& get_handle_internal(handle_type handle)
    {
        return std::get<handle_type>(data[handle.value()]);
    }

    T& get_value_internal(handle_type handle)
    {
        return std::get<T>(data[handle.value()]);
    }

public:
    Arena() = default;
    Arena(usize capacity)
    {
        data.reserve(capacity);
    }

    handle_type add(T&& value)
    {
        if (!first_free.is_valid())
        {
            data.push_back(std::move(value));
            return handle_type(data.size() - 1);
        }

        // Pop the free list
        handle_type old_first_free = first_free;
        first_free = get_handle_internal(old_first_free);

        //
        element_type new_elem = std::move(value);
        data[old_first_free.value()] = std::move(new_elem);

        return old_first_free;
    }

    T* get(handle_type handle)
    {
        if (!handle.is_valid()) {
            return nullptr;
        }

        return &get_value_internal(handle);
    }

    void remove(handle_type handle)
    {
        if (!handle.is_valid()) {
            return;
        }

        auto& element = data[handle.value()];
        element = first_free;
        first_free = handle;
    }

private:
    handle_type first_free;
    std::vector<element_type> data;
};

} // namespace my_app
