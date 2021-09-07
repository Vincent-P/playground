#include "exo/matrices.h"

#include "exo/types.h"
#include <cstring>

float4x4::float4x4(float value)
{
    for (auto &uninit : values)
    {
        uninit = 0.0f;
    }

    values[0]  = value;
    values[5]  = value;
    values[10] = value;
    values[15] = value;
}

float4x4::float4x4(const float (&_values)[16])
{
    for (uint col = 0; col < 4; col++)
    {
        for (uint row = 0; row < 4; row++)
        {
            this->at(row, col) = _values[row * 4 + col];
        }
    }
}

float4x4 float4x4::identity()
{
    return float4x4(1.0f);
}

const float &float4x4::at(usize row, usize col) const
{
    assert(row < 4 && col < 4);
    // values are stored in columns
    return values[col * 4 + row];
}

float &float4x4::at(usize row, usize col)
{
    assert(row < 4 && col < 4);
    // values are stored in columns
    return values[col * 4 + row];
}

const float4 &float4x4::col(usize col) const
{
    assert(col < 4);
    return *reinterpret_cast<const float4 *>(&values[col * 4]);
}

float4 &float4x4::col(usize col)
{
    assert(col < 4);
    return *reinterpret_cast<float4 *>(&values[col * 4]);
}

float4x4 transpose(const float4x4 &m)
{
    float4x4 result;
    for (usize row = 0; row < 4; row++)
    {
        for (usize col = 0; col < 4; col++)
        {
            result.at(row, col) = m.at(col, row);
        }
    }
    return result;
}

float4x4 operator*(float a, const float4x4 &m)
{
    float4x4 result;
    for (usize i = 0; i < 16; i++)
    {
        result.values[i] = a * m.values[i];
    }
    return result;
}

float4 operator*(const float4x4 &m, const float4 &v)
{
    float4 result = {0.0f};
    for (usize row = 0; row < 4; row++)
    {
        for (usize col = 0; col < 4; col++)
        {
            result[row] += m.at(row, col) * v[col];
        }
    }
    return result;
}

bool operator==(const float4x4 &a, const float4x4 &b)
{
    return std::memcmp(a.values, b.values, sizeof(a.values)) == 0;
}

float4x4 operator+(const float4x4 &a, const float4x4 &b)
{
    float4x4 result;
    for (usize col = 0; col < 4; col++)
    {
        for (usize row = 0; row < 4; row++)
        {
            result.at(row, col) = a.at(row, col) + b.at(row, col);
        }
    }
    return result;
}

float4x4 operator-(const float4x4 &a, const float4x4 &b)
{
    float4x4 result;
    for (usize col = 0; col < 4; col++)
    {
        for (usize row = 0; row < 4; row++)
        {
            result.at(row, col) = a.at(row, col) - b.at(row, col);
        }
    }
    return result;
}

float4x4 operator*(const float4x4 &a, const float4x4 &b)
{
    float4x4 result;
    for (usize col = 0; col < 4; col++)
    {
        for (usize row = 0; row < 4; row++)
        {
            for (usize i = 0; i < 4; i++)
            {
                result.at(row, col) += a.at(row, i) * b.at(i, col);
            }
        }
    }
    return result;
}
