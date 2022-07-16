#include "exo/maths/matrices.h"

#include "exo/macros/assert.h"
#include <cstring> // for std::memcmp

namespace exo
{
float4x4::float4x4(float value)
{
	for (auto &uninit : values) {
		uninit = 0.0f;
	}

	values[0]  = value;
	values[5]  = value;
	values[10] = value;
	values[15] = value;
}

float4x4::float4x4(const float (&_values)[16])
{
	for (uint col = 0; col < 4; col++) {
		for (uint row = 0; row < 4; row++) {
			this->at(row, col) = _values[row * 4 + col];
		}
	}
}

float4x4 float4x4::identity() { return float4x4(1.0f); }

const float &float4x4::at(usize row, usize col) const
{
	ASSERT(row < 4 && col < 4);
	// values are stored in columns
	return values[col * 4 + row];
}

float &float4x4::at(usize row, usize col)
{
	return const_cast<float &>(static_cast<const float4x4 &>(*this).at(row, col));
}

const float4 &float4x4::col(usize col) const
{
	ASSERT(col < 4);
	return *reinterpret_cast<const float4 *>(&values[col * 4]);
}

float4 &float4x4::col(usize col) { return const_cast<float4 &>(static_cast<const float4x4 &>(*this).col(col)); }

float4x4 transpose(const float4x4 &m)
{
	float4x4 result;
	for (usize row = 0; row < 4; row++) {
		for (usize col = 0; col < 4; col++) {
			result.at(row, col) = m.at(col, row);
		}
	}
	return result;
}

float4x4 operator*(float a, const float4x4 &m)
{
	float4x4 result;
	for (usize i = 0; i < 16; i++) {
		result.values[i] = a * m.values[i];
	}
	return result;
}

bool operator==(const float4x4 &a, const float4x4 &b) { return std::memcmp(a.values, b.values, sizeof(a.values)) == 0; }

float4x4 operator+(const float4x4 &a, const float4x4 &b)
{
	float4x4 result;
	for (usize col = 0; col < 4; col++) {
		for (usize row = 0; row < 4; row++) {
			result.at(row, col) = a.at(row, col) + b.at(row, col);
		}
	}
	return result;
}

float4x4 operator-(const float4x4 &a, const float4x4 &b)
{
	float4x4 result;
	for (usize col = 0; col < 4; col++) {
		for (usize row = 0; row < 4; row++) {
			result.at(row, col) = a.at(row, col) - b.at(row, col);
		}
	}
	return result;
}

float4x4 operator*(const float4x4 &a, const float4x4 &b)
{
	float4x4 result;
	result.at(0, 0) =
		a.at(0, 0) * b.at(0, 0) + a.at(0, 1) * b.at(1, 0) + a.at(0, 2) * b.at(2, 0) + a.at(0, 3) * b.at(3, 0);
	result.at(1, 0) =
		a.at(1, 0) * b.at(0, 0) + a.at(1, 1) * b.at(1, 0) + a.at(1, 2) * b.at(2, 0) + a.at(1, 3) * b.at(3, 0);
	result.at(2, 0) =
		a.at(2, 0) * b.at(0, 0) + a.at(2, 1) * b.at(1, 0) + a.at(2, 2) * b.at(2, 0) + a.at(2, 3) * b.at(3, 0);
	result.at(3, 0) =
		a.at(3, 0) * b.at(0, 0) + a.at(3, 1) * b.at(1, 0) + a.at(3, 2) * b.at(2, 0) + a.at(3, 3) * b.at(3, 0);

	result.at(0, 1) =
		a.at(0, 0) * b.at(0, 1) + a.at(0, 1) * b.at(1, 1) + a.at(0, 2) * b.at(2, 1) + a.at(0, 3) * b.at(3, 1);
	result.at(1, 1) =
		a.at(1, 0) * b.at(0, 1) + a.at(1, 1) * b.at(1, 1) + a.at(1, 2) * b.at(2, 1) + a.at(1, 3) * b.at(3, 1);
	result.at(2, 1) =
		a.at(2, 0) * b.at(0, 1) + a.at(2, 1) * b.at(1, 1) + a.at(2, 2) * b.at(2, 1) + a.at(2, 3) * b.at(3, 1);
	result.at(3, 1) =
		a.at(3, 0) * b.at(0, 1) + a.at(3, 1) * b.at(1, 1) + a.at(3, 2) * b.at(2, 1) + a.at(3, 3) * b.at(3, 1);

	result.at(0, 2) =
		a.at(0, 0) * b.at(0, 2) + a.at(0, 1) * b.at(1, 2) + a.at(0, 2) * b.at(2, 2) + a.at(0, 3) * b.at(3, 2);
	result.at(1, 2) =
		a.at(1, 0) * b.at(0, 2) + a.at(1, 1) * b.at(1, 2) + a.at(1, 2) * b.at(2, 2) + a.at(1, 3) * b.at(3, 2);
	result.at(2, 2) =
		a.at(2, 0) * b.at(0, 2) + a.at(2, 1) * b.at(1, 2) + a.at(2, 2) * b.at(2, 2) + a.at(2, 3) * b.at(3, 2);
	result.at(3, 2) =
		a.at(3, 0) * b.at(0, 2) + a.at(3, 1) * b.at(1, 2) + a.at(3, 2) * b.at(2, 2) + a.at(3, 3) * b.at(3, 2);

	result.at(0, 3) =
		a.at(0, 0) * b.at(0, 3) + a.at(0, 1) * b.at(1, 3) + a.at(0, 2) * b.at(2, 3) + a.at(0, 3) * b.at(3, 3);
	result.at(1, 3) =
		a.at(1, 0) * b.at(0, 3) + a.at(1, 1) * b.at(1, 3) + a.at(1, 2) * b.at(2, 3) + a.at(1, 3) * b.at(3, 3);
	result.at(2, 3) =
		a.at(2, 0) * b.at(0, 3) + a.at(2, 1) * b.at(1, 3) + a.at(2, 2) * b.at(2, 3) + a.at(2, 3) * b.at(3, 3);
	result.at(3, 3) =
		a.at(3, 0) * b.at(0, 3) + a.at(3, 1) * b.at(1, 3) + a.at(3, 2) * b.at(2, 3) + a.at(3, 3) * b.at(3, 3);
	return result;
}

float4 operator*(const float4x4 &m, const float4 &v)
{
	float4 result = {0.0f};
	result[0]     = m.at(0, 0) * v[0] + m.at(0, 1) * v[1] + m.at(0, 2) * v[2] + m.at(0, 3) * v[3];
	result[1]     = m.at(1, 0) * v[0] + m.at(1, 1) * v[1] + m.at(1, 2) * v[2] + m.at(1, 3) * v[3];
	result[2]     = m.at(2, 0) * v[0] + m.at(2, 1) * v[1] + m.at(2, 2) * v[2] + m.at(2, 3) * v[3];
	result[3]     = m.at(3, 0) * v[0] + m.at(3, 1) * v[1] + m.at(3, 2) * v[2] + m.at(3, 3) * v[3];
	return result;
}

float4x4 inverse_transform(const float4x4 &transform)
{
	float4x4 reversed_transform = transform;

	// -- Extract the translation, and scale from the matrix
	float3 translation        = transform.col(3).xyz();
	reversed_transform.col(3) = float4(0.0f, 0.0f, 0.0f, 1.0f);

	float3 scale;
	scale.x = length(reversed_transform.col(0));
	scale.y = length(reversed_transform.col(1));
	scale.z = length(reversed_transform.col(2));

	reversed_transform.at(0, 0) /= scale.x;
	reversed_transform.at(1, 0) /= scale.x;
	reversed_transform.at(2, 0) /= scale.x;

	reversed_transform.at(0, 1) /= scale.y;
	reversed_transform.at(1, 1) /= scale.y;
	reversed_transform.at(2, 1) /= scale.y;

	reversed_transform.at(0, 2) /= scale.z;
	reversed_transform.at(1, 2) /= scale.z;
	reversed_transform.at(2, 2) /= scale.z;

	// -- The matrix is now only a rotation transform

	// Invert it with a transpose
	reversed_transform = transpose(reversed_transform);

	// Rotate the inverse translation
	translation               = -1.0f * (reversed_transform * float4(translation, 1.0f)).xyz();
	reversed_transform.col(3) = float4(translation, 1.0f);

	// Divide by the scale
	reversed_transform.at(0, 0) /= scale.x;
	reversed_transform.at(0, 1) /= scale.x;
	reversed_transform.at(0, 2) /= scale.x;
	reversed_transform.at(0, 3) /= scale.x;

	reversed_transform.at(1, 0) /= scale.y;
	reversed_transform.at(1, 1) /= scale.y;
	reversed_transform.at(1, 2) /= scale.y;
	reversed_transform.at(1, 3) /= scale.y;

	reversed_transform.at(2, 0) /= scale.z;
	reversed_transform.at(2, 1) /= scale.z;
	reversed_transform.at(2, 2) /= scale.z;
	reversed_transform.at(2, 3) /= scale.z;

	return reversed_transform;
}
} // namespace exo
