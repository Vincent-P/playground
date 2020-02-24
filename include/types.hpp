#pragma once
#include <cstdint>

#define NO_COPY_NO_MOVE(name) \
	name(const name& other) = delete; \
	name(const name&& other) = delete; \
	name operator =(const name& other) = delete; \
	name operator =(const name&& other) = delete;

namespace my_app
{
    using u8 = uint8_t;
    using u32 = uint32_t;
    using u64 = uint64_t;
    using usize = size_t;

    static constexpr u32 u32_invalid = ~0lu;

    template<typename T>
    struct Handle
    {
	static Handle invalid() { return u32_invalid; }
	Handle() : index(u32_invalid) {}
	explicit Handle(u32 i) : index(i) {}

	u32 value() { return index; }

	friend bool operator==(Handle a, Handle b) { return a.index == b.index; }
	friend bool operator!=(Handle a, Handle b) { return a.index != b.index; }

    private:
	u32 index;
    };
}
