#pragma once
#include "exo/maths/matrices.h"

namespace exo
{
// https://fabiensanglard.net/doom3_documentation/37726-293748.pdf
inline float4x4 float4x4_from_quaternion(float4 q)
{
	float x2 = q.x + q.x;
	float y2 = q.y + q.y;
	float z2 = q.z + q.z;
	// float w2  = q.w + q.w;
	float yy2 = q.y * y2;
	float xy2 = q.x * y2;
	float xz2 = q.x * z2;
	float yz2 = q.y * z2;
	float zz2 = q.z * z2;
	float wz2 = q.w * z2;
	float wy2 = q.w * y2;
	float wx2 = q.w * x2;
	float xx2 = q.x * x2;

	float4x4 m;
	m.at(0, 0) = -yy2 - zz2 + 1.0f;
	m.at(0, 1) = xy2 + wz2;
	m.at(0, 2) = xz2 - wy2;
	m.at(0, 3) = 0.0f;

	m.at(1, 0) = xy2 - wz2;
	m.at(1, 1) = -xx2 - zz2 + 1.0f;
	m.at(1, 2) = yz2 + wx2;
	m.at(1, 3) = 0.0f;

	m.at(2, 0) = xz2 + wy2;
	m.at(2, 1) = yz2 - wx2;
	m.at(2, 2) = -xx2 - yy2 + 1.0f;
	m.at(2, 3) = 0.0f;

	m.at(3, 0) = 0.0f;
	m.at(3, 1) = 0.0f;
	m.at(3, 2) = 0.0f;
	m.at(3, 3) = 1.0f;
	return m;
}

static float ReciprocalSqrt(float x)
{
	long  i;
	float y, r;
	y = x * 0.5f;
	i = *(long *)(&x);
	i = 0x5f3759df - (i >> 1);
	r = *(float *)(&i);
	r = r * (1.5f - r * r * y);
	return r;
}

inline float4 quaternion_from_float4x4(float4x4 m)
{
	float4 q = {0.0f};

	if (m.at(0, 0) + m.at(1, 1) + m.at(2, 2) > 0.0f) {
		float t = +m.at(0, 0) + m.at(1, 1) + m.at(2, 2) + 1.0f;
		float s = ReciprocalSqrt(t) * 0.5f;
		q.w     = s * t;
		q.z     = (m.at(0, 1) - m.at(1, 0)) * s;
		q.y     = (m.at(2, 0) - m.at(0, 2)) * s;
		q.x     = (m.at(1, 2) - m.at(2, 1)) * s;
	} else if (m.at(0, 0) > m.at(1, 1) && m.at(0, 0) > m.at(2, 2)) {
		float t = +m.at(0, 0) - m.at(1, 1) - m.at(2, 2) + 1.0f;
		float s = ReciprocalSqrt(t) * 0.5f;
		q.x     = s * t;
		q.y     = (m.at(0, 1) + m.at(1, 0)) * s;

		q.z = (m.at(2, 0) + m.at(0, 2)) * s;
		q.w = (m.at(1, 2) - m.at(2, 1)) * s;
	} else if (m.at(1, 1) > m.at(2, 2)) {
		float t = -m.at(0, 0) + m.at(1, 1) - m.at(2, 2) + 1.0f;
		float s = ReciprocalSqrt(t) * 0.5f;
		q.y     = s * t;
		q.x     = (m.at(0, 1) + m.at(1, 0)) * s;
		q.w     = (m.at(2, 0) - m.at(0, 2)) * s;
		q.z     = (m.at(1, 2) + m.at(2, 1)) * s;
	} else {
		float t = -m.at(0, 0) - m.at(1, 1) + m.at(2, 2) + 1.0f;
		float s = ReciprocalSqrt(t) * 0.5f;
		q.z     = s * t;
		q.w     = (m.at(0, 1) - m.at(1, 0)) * s;
		q.x     = (m.at(2, 0) + m.at(0, 2)) * s;
		q.y     = (m.at(1, 2) + m.at(2, 1)) * s;
	}

	return q;
}
} // namespace exo
