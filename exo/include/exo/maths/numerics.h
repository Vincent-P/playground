#pragma once
#include <cstddef>
#include <cstdint>

using i8    = std::int8_t;
using i16   = std::int16_t;
using i32   = std::int32_t;
using i64   = std::int64_t;
using u8    = std::uint8_t;
using u16   = std::uint16_t;
using u32   = std::uint32_t;
using u64   = std::uint64_t;
using usize = std::size_t;
using isize = std::ptrdiff_t;
using uchar = unsigned char;
using uint  = unsigned int;
using f32   = float;
using f64   = double;

constexpr inline u64   operator"" _K(unsigned long long value) { return value * 1000u; }
constexpr inline u64   operator"" _KiB(unsigned long long value) { return value << 10; }
constexpr inline u64   operator"" _MiB(unsigned long long value) { return value << 20; }
constexpr inline u64   operator"" _GiB(unsigned long long value) { return value << 30; }
constexpr inline usize operator"" _uz(unsigned long long value) { return static_cast<usize>(value); }

static constexpr u32 u32_invalid = ~u32(0);
static constexpr u64 u64_invalid = ~u64(0);

constexpr f32 PI = 3.1415926535897932384626433832795f;

namespace exo
{
constexpr f32 to_radians(f32 degres)
{
	// 180    -> PI
	// degres -> ?
	return degres * PI / 180.0f;
}

constexpr f64 to_radians(f64 degres)
{
	// 180    -> PI
	// degres -> ?
	return degres * PI / 180.0;
}
} // namespace exo
