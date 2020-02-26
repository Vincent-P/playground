#pragma once
#include <cstddef>
#include <cstdint>

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

using i8    = std::int8_t;
using i32   = std::int32_t;
using i64   = std::int64_t;
using u8    = std::uint8_t;
using u32   = std::uint32_t;
using u64   = std::uint64_t;
using usize = std::size_t;
using uchar = unsigned char;
using uint  = unsigned int;

static constexpr u32 u32_invalid = ~0u;

template <typename T> inline T *ptr_offset(T *ptr, usize offset) { return reinterpret_cast<char *>(ptr) + offset; }

template <typename T> struct Handle
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

} // namespace my_app
