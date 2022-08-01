#pragma once
#include "exo/collections/vector.h"
#include "exo/macros/assert.h"
#include "exo/maths/numerics.h"

#include <span>

namespace exo
{
struct StringRepository;
struct ScopeStack;
struct float4x4;
struct float4;
struct float2;
struct int2;
struct Serializer;

template <typename T>
concept MemberSerializable = requires(T &a)
{
	// clang-format off
	{ a.serialize(*(Serializer *)nullptr) } -> std::same_as<void>;
	// clang-format on
};

struct Serializer
{
	static Serializer create(ScopeStack *s = nullptr, StringRepository *r = nullptr);

	void read_bytes(void *dst, usize len);
	void write_bytes(const void *src, usize len);

	StringRepository *str_repo;
	ScopeStack       *scope;
	i32               version;
	bool              is_writing;
	void             *buffer;
	usize             offset;
	usize             buffer_size;
};

template <MemberSerializable T> void serialize(Serializer &serializer, T &data) { data.serialize(serializer); }

template <typename T, usize n> void serialize(Serializer &serializer, T (&data)[n])
{
	usize size = n;
	serialize(serializer, size);
	ASSERT(size == n);

	for (usize i = 0; i < size; i += 1) {
		serialize(serializer, data[i]);
	}
}

template <typename T> void serialize(Serializer &serializer, Vec<T> &data)
{
	usize size = data.size();
	serialize(serializer, size);

	if (serializer.is_writing == false) {
		data.resize(size);
	}

	ASSERT(size == data.size());
	for (usize i = 0; i < size; i += 1) {
		serialize(serializer, data[i]);
	}
}

// builtin types
void serialize(Serializer &serializer, i8 &data);
void serialize(Serializer &serializer, i16 &data);
void serialize(Serializer &serializer, i32 &data);
void serialize(Serializer &serializer, i64 &data);
void serialize(Serializer &serializer, u8 &data);
void serialize(Serializer &serializer, u16 &data);
void serialize(Serializer &serializer, u32 &data);
void serialize(Serializer &serializer, u64 &data);

void serialize(Serializer &serializer, f32 &data);
void serialize(Serializer &serializer, f64 &data);

void serialize(Serializer &serializer, char &data);
void serialize(Serializer &serializer, bool &data);

// interned string
void serialize(Serializer &serializer, const char *&data);

// vectors
void serialize(Serializer &serializer, float4x4 &data);
void serialize(Serializer &serializer, float4 &data);
void serialize(Serializer &serializer, float2 &data);
void serialize(Serializer &serializer, int2 &data);

} // namespace exo
