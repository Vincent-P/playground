#pragma once
#include <cstdint>

#define NO_COPY_NO_MOVE(name) \
    name(const name& other) = delete; \
    name(const name&& other) = delete; \
    name operator =(const name& other) = delete; \
    name operator =(const name&& other) = delete;

#define VK_CHECK(x)                                         \
    do                                                      \
    {                                                       \
        VkResult err = x;                                   \
        if (err)                                            \
        {                                                   \
            std::string error("Vulkan error");              \
            error = std::to_string(err) + std::string("."); \
            std::cerr << error << std::endl;                \
            throw std::runtime_error(error);                \
        }                                                   \
    } while (0)

#define ARRAY_SIZE(_arr) (sizeof(_arr) / sizeof(*_arr))

namespace my_app
{
    using i8 = int8_t;
    using i32 = int32_t;
    using i64 = int64_t;
    using u8 = uint8_t;
    using u32 = uint32_t;
    using u64 = uint64_t;
    using usize = size_t;
    using uchar = unsigned char;
    using uint = unsigned int;

    static constexpr u32 u32_invalid = ~0lu;

    template<typename T>
    inline T* ptr_offset(T* ptr, usize offset)
    {
        return reinterpret_cast<char*>(ptr) + offset;
    }

    template<typename T>
    struct Handle
    {
    static Handle invalid() { return Handle(u32_invalid); }
    Handle() : index(u32_invalid) {}
    explicit Handle(u32 i) : index(i) {}

    u32 value() const { return index; }
    bool is_valid() const { return *this != invalid(); }

    friend bool operator==(Handle a, Handle b) { return a.index == b.index; }
    friend bool operator!=(Handle a, Handle b) { return a.index != b.index; }

    private:
    u32 index;
    };
}
