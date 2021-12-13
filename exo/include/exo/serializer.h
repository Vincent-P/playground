#pragma once
#include "exo/macros/assert.h"
#include "exo/maths/numerics.h"
#include "exo/collections/vector.h"

namespace exo
{
struct StringRepository;
struct ScopeStack;
struct float4x4;
struct float4;
struct float2;
struct int2;

struct Serializer
{
    static Serializer create(ScopeStack *s = nullptr, StringRepository *r = nullptr);

    void read_bytes(void *dst, usize len);
    void write_bytes(const void *src, usize len);

    template <typename T>
    void serialize(T &data);

    template <typename T, usize n>
    void serialize(T (&data)[n]);

    template <typename T>
    void serialize(Vec<T> &data);

    // builtin types
    void serialize(i8 &data);
    void serialize(i16 &data);
    void serialize(i32 &data);
    void serialize(i64 &data);
    void serialize(u8 &data);
    void serialize(u16 &data);
    void serialize(u32 &data);
    void serialize(u64 &data);

    void serialize(f32 &data);
    void serialize(f64 &data);

    void serialize(char &data);
    void serialize(bool &data);

    // interned string
    void serialize(const char *&data);

    // vectors
    void serialize(exo::float4x4 &data);
    void serialize(exo::float4 &data);
    void serialize(exo::float2 &data);
    void serialize(exo::int2 &data);

    StringRepository *str_repo;
    ScopeStack       *scope;
    i32               version;
    bool              is_writing;
    void             *buffer;
    usize             offset;
    usize             buffer_size;
};

template <typename T, usize n>
void Serializer::serialize(T (&data)[n])
{
    usize size = n;
    serialize(size);
    ASSERT(size == n);

    for (usize i = 0; i < size; i += 1)
    {
        serialize(data[i]);
    }
}

template <typename T>
void Serializer::serialize(Vec<T> &data)
{
    usize size = data.size();
    serialize(size);

    if (this->is_writing == false)
    {
        data.resize(size);
    }

    ASSERT(size == data.size());
    for (usize i = 0; i < size; i += 1)
    {
        serialize(data[i]);
    }
}

} // namespace exo
