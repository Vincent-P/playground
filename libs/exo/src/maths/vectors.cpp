#include "exo/maths/vectors.h"

#include <algorithm> // for std::min,max :/
#include <cmath>     // for std::round

#if defined(ENABLE_DOCTEST)
#include <doctest.h>
#endif

namespace exo
{
template <typename V>
auto max_impl(V lhs, V rhs)
{
	V res;
	for (usize i_component = 1; i_component < V::SIZE; i_component += 1) {
		res[i_component] = std::max(lhs[i_component], rhs[i_component]);
	}
	return res;
}

template <typename V>
auto min_impl(V lhs, V rhs)
{
	V res;
	for (usize i_component = 1; i_component < V::SIZE; i_component += 1) {
		res[i_component] = std::min(lhs[i_component], rhs[i_component]);
	}
	return res;
}

template <typename V>
float dot_impl(V a, V b)
{
	float res = 0.0f;
	for (usize i_component = 0; i_component < V::SIZE; i_component += 1) {
		res += a[i_component] * b[i_component];
	}
	return res;
}

template <typename V>
float length_impl(V a)
{
	const float squared_length = dot_impl(a, a);
	return std::sqrt(squared_length);
}

template <typename V>
float distance_impl(V a, V b)
{
	return length_impl(b - a);
}

template <typename V>
V normalize_impl(V a)
{
	const float inverse_len = 1.0f / length_impl(a);
	return inverse_len * a;
}

template <typename V>
V round_impl(V a)
{
	V res = V{0};
	for (usize i_component = 0; i_component < V::SIZE; i_component += 1) {
		res[i_component] = std::round(a[i_component]);
	}
	return res;
}

template <typename V>
V floor_impl(V a)
{
	V res = V{0};
	for (usize i_component = 0; i_component < V::SIZE; i_component += 1) {
		res[i_component] = std::floor(a[i_component]);
	}
	return res;
}

template <typename V>
V ceil_impl(V a)
{
	V res = V{0};
	for (usize i_component = 0; i_component < V::SIZE; i_component += 1) {
		res[i_component] = std::ceil(a[i_component]);
	}
	return res;
}

// clang-format off
float2 max(float2 lhs, float2 rhs) { return max_impl(lhs, rhs); }
float2 min(float2 lhs, float2 rhs) { return min_impl(lhs, rhs); }

float length(float2 v) { return length_impl(v); }
float length(float3 v) { return length_impl(v); }
float length(float4 v) { return length_impl(v); }

float distance(float2 a, float2 b) { return distance_impl(a, b); }
float distance(float3 a, float3 b) { return distance_impl(a, b); }
float distance(float4 a, float4 b) { return distance_impl(a, b); }

float dot(float2 a, float2 b) { return dot_impl(a, b); }
float dot(float3 a, float3 b) { return dot_impl(a, b); }
float dot(float4 a, float4 b) { return dot_impl(a, b); }

float2 normalize(float2 v) { return normalize_impl(v); }
float3 normalize(float3 v) { return normalize_impl(v); }
float4 normalize(float4 v) { return normalize_impl(v); }

float2 round(float2 v) { return round_impl(v); }
float3 round(float3 v) { return round_impl(v); }
float4 round(float4 v) { return round_impl(v); }

float2 floor(float2 v) { return floor_impl(v); }
float3 floor(float3 v) { return floor_impl(v); }
float4 floor(float4 v) { return floor_impl(v); }

float2 ceil(float2 v) { return ceil_impl(v); }
float3 ceil(float3 v) { return ceil_impl(v); }
float4 ceil(float4 v) { return ceil_impl(v); }
// clang-format on

float3 cross(float3 a, float3 b) { return {a.y * b.z - b.y * a.z, a.z * b.x - b.z * a.x, a.x * b.y - b.x * a.y}; }

/// --- Tests

#if defined(ENABLE_DOCTEST)
namespace test
{
TEST_SUITE("Vectors")
{
	TEST_CASE("operators")
	{
		CHECK(float2(1.0f, 2.0f) + float2(3.0f, 4.0f) == float2(4.0f, 6.0f));
		CHECK(float2(1.0f, 2.0f) - float2(3.0f, 4.0f) == float2(-2.0f));
		CHECK(float2(1.0f, 2.0f) * float2(3.0f, 4.0f) == float2(3.0f, 8.0f));

		CHECK(float3(1.0f, 2.0f, 3.0f) + float3(4.0f, 5.0f, 6.0f) == float3(5.0f, 7.0f, 9.0f));
		CHECK(float3(1.0f, 2.0f, 3.0f) - float3(4.0f, 5.0f, 6.0f) == float3(-3.0f));
		CHECK(float3(1.0f, 2.0f, 3.0f) * float3(4.0f, 5.0f, 6.0f) == float3(4.0f, 10.0f, 18.0f));

		CHECK(float4(1.0f, 2.0f, 3.0f, 4.0f) + float4(5.0f, 6.0f, 7.0f, 8.0f) == float4(6.0f, 8.0f, 10.0f, 12.0f));
		CHECK(float4(1.0f, 2.0f, 3.0f, 4.0f) - float4(5.0f, 6.0f, 7.0f, 8.0f) == float4(-4.0f));
		CHECK(float4(1.0f, 2.0f, 3.0f, 4.0f) * float4(5.0f, 6.0f, 7.0f, 8.0f) == float4(5.0f, 12.0f, 21.0f, 32.0f));
	}

	TEST_CASE("length")
	{
		CHECK(dot(float2{1.0f, 2.0f}, float2{1.0f, 2.0f}) == doctest::Approx(5.0f));
		CHECK(length(float2(1.0f, 2.0f)) == doctest::Approx(sqrt(5.0f)));

		CHECK(dot(float3(1.0f, 2.0f, 3.0f), float3(1.0f, 2.0f, 3.0f)) == doctest::Approx(14.0f));
		CHECK(length(float3(1.0f, 2.0f, 3.0f)) == doctest::Approx(sqrt(14.0f)));

		CHECK(dot(float4(1.0f, 2.0f, 3.0f, 4.0f), float4(1.0f, 2.0f, 3.0f, 4.0f)) == doctest::Approx(30.0f));
		CHECK(length(float4(1.0f, 2.0f, 3.0f, 4.0f)) == doctest::Approx(sqrt(30.0f)));
	}

	TEST_CASE("maths")
	{
		auto v2 = float2(1.0f, 2.0f);
		auto v3 = float3(1.0f, 2.0f, 3.0f);
		auto v4 = float4(1.0f, 2.0f, 3.0f, 4.0f);

		CHECK(dot(v2, float2(1.0f, 0.0f)) == 1.0f);
		CHECK(dot(v2, float2(0.0f, 1.0f)) == 2.0f);
		CHECK(length(normalize(v2)) == doctest::Approx(1.0f)); // approx compares with a small epsilon
		CHECK(round(1.5f * v2) == float2(2.0f, 3.0f));

		CHECK(dot(v3, float3(1.0f, 0.0f, 0.0f)) == 1.0f);
		CHECK(dot(v3, float3(0.0f, 1.0f, 0.0f)) == 2.0f);
		CHECK(dot(v3, float3(0.0f, 0.0f, 1.0f)) == 3.0f);
		CHECK(length(normalize(v3)) == doctest::Approx(1.0f)); // approx compares with a small epsilon
		CHECK(round(1.5f * v3) == float3(2.0f, 3.0f, 5.0f));

		CHECK(dot(v4, float4(1.0f, 0.0f, 0.0f, 0.0f)) == 1.0f);
		CHECK(dot(v4, float4(0.0f, 1.0f, 0.0f, 0.0f)) == 2.0f);
		CHECK(dot(v4, float4(0.0f, 0.0f, 1.0f, 0.0f)) == 3.0f);
		CHECK(dot(v4, float4(0.0f, 0.0f, 0.0f, 1.0f)) == 4.0f);
		CHECK(length(normalize(v4)) == doctest::Approx(1.0f)); // approx compares with a small epsilon
		CHECK(round(1.5f * v4) == float4(2.0f, 3.0f, 5.0f, 6.0f));
	}

	TEST_CASE("cross product")
	{
		float3 x{1.0f, 0.0f, 0.0f};
		float3 y{0.0f, 1.0f, 0.0f};
		float3 z{0.0f, 0.0f, 1.0f};

		CHECK(cross(x, y) == z);
		CHECK(cross(y, z) == x);
		CHECK(cross(z, x) == y);

		CHECK(cross(y, x) == -1.0f * z);
		CHECK(cross(z, y) == -1.0f * x);
		CHECK(cross(x, z) == -1.0f * y);
	}

	TEST_CASE("raw access")
	{
		float3 x{1.0f, 0.0f, 0.0f};
		const float3 y{0.0f, 1.0f, 0.0f};
		const float3 z{0.0f, 0.0f, 1.0f};

		x[0] = 3.0f;

		CHECK(x.x == 3.0f);
	}
}

TEST_SUITE("Matrices")
{
	TEST_CASE("operators")
	{
		float4x4 identity = float4x4::identity();
		float4x4 expected{{
			2.0f,
			0.0f,
			0.0f,
			0.0f,
			0.0f,
			2.0f,
			0.0f,
			0.0f,
			0.0f,
			0.0f,
			2.0f,
			0.0f,
			0.0f,
			0.0f,
			0.0f,
			2.0f,
		}};
		CHECK(2.0f * identity == expected);
	}

	TEST_CASE("Matrix multiplication")
	{
		float4x4 identity({
			1.0f,
			0.0f,
			0.0f,
			0.0f,
			0.0f,
			1.0f,
			0.0f,
			0.0f,
			0.0f,
			0.0f,
			1.0f,
			0.0f,
			0.0f,
			0.0f,
			0.0f,
			1.0f,
		});

		float4x4 m({
			1.0f,
			2.0f,
			0.0f,
			0.0f,
			2.0f,
			1.0f,
			4.0f,
			0.0f,
			0.0f,
			9.0f,
			1.0f,
			0.0f,
			3.0f,
			0.0f,
			8.0f,
			1.0f,
		});

		CHECK(identity * (m * identity) == m);

		float4 v{1.0f, 2.0f, 3.0f, 4.0f};

		float4 expected{5.0f, 16.0f, 21.0f, 31.0f};
		CHECK(m * v == expected);
	}

	TEST_CASE("Single element access")
	{
		float4x4 m({
			1.0f,
			2.0f,
			3.0f,
			4.0f,
			5.0f,
			6.0f,
			7.0f,
			8.0f,
			9.0f,
			10.0f,
			11.0f,
			12.0f,
			13.0f,
			14.0f,
			15.0f,
			16.0f,
		});

		CHECK(m.at(0, 0) == 1.0f);
		CHECK(m.at(0, 1) == 2.0f);
		CHECK(m.at(0, 2) == 3.0f);
		CHECK(m.at(0, 3) == 4.0f);
		CHECK(m.at(1, 0) == 5.0f);
		CHECK(m.at(1, 1) == 6.0f);
		CHECK(m.at(1, 2) == 7.0f);
		CHECK(m.at(1, 3) == 8.0f);
		CHECK(m.at(2, 0) == 9.0f);
		CHECK(m.at(2, 1) == 10.0f);
		CHECK(m.at(2, 2) == 11.0f);
		CHECK(m.at(2, 3) == 12.0f);
		CHECK(m.at(3, 0) == 13.0f);
		CHECK(m.at(3, 1) == 14.0f);
		CHECK(m.at(3, 2) == 15.0f);
		CHECK(m.at(3, 3) == 16.0f);
	}

	TEST_CASE("Column access")
	{
		float4x4 m({
			1.0f,
			2.0f,
			3.0f,
			4.0f,
			5.0f,
			6.0f,
			7.0f,
			8.0f,
			9.0f,
			10.0f,
			11.0f,
			12.0f,
			13.0f,
			14.0f,
			15.0f,
			16.0f,
		});

		CHECK(m.col(0) == float4(1, 5, 9, 13));
		CHECK(m.col(1) == float4(2, 6, 10, 14));
		CHECK(m.col(2) == float4(3, 7, 11, 15));
		CHECK(m.col(3) == float4(4, 8, 12, 16));
	}
}
} // namespace test
#endif
} // namespace exo
