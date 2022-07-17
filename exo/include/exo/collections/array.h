#pragma once
namespace exo::Array
{
template <typename T, size_t N> using Reference = T (&)[N];
template <typename T, size_t N> [[nodiscard]] inline constexpr size_t len(Reference<const T, N> _) { return N; }
template <typename T, size_t N> struct AsStruct
{
	T inner[N];
};
} // namespace exo::Array
