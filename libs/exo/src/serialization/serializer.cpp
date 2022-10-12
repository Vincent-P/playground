#include "exo/serialization/serializer.h"

#include "exo/maths/matrices.h"
#include "exo/maths/vectors.h"
#include "exo/memory/scope_stack.h"
#include "exo/memory/string_repository.h"

namespace exo
{
Serializer Serializer::create(ScopeStack *s, StringRepository *r)
{
	Serializer result  = {};
	result.str_repo    = r ? r : tls_string_repository;
	result.scope       = s;
	result.version     = 1;
	result.is_writing  = false;
	result.buffer      = nullptr;
	result.offset      = 0;
	result.buffer_size = 0;
	return result;
}

void Serializer::read_bytes(void *dst, usize len)
{
	ASSERT(this->is_writing == false);
	usize aligned_len = round_up_to_alignment(sizeof(u32), len);
	ASSERT(this->offset + aligned_len <= this->buffer_size);
	std::memcpy(dst, ptr_offset(this->buffer, this->offset), len);
	offset += aligned_len;
}

void Serializer::write_bytes(const void *src, usize len)
{
	ASSERT(this->is_writing == true);
	usize aligned_len = round_up_to_alignment(sizeof(u32), len);
	ASSERT(this->offset + aligned_len <= this->buffer_size);
	std::memcpy(ptr_offset(this->buffer, this->offset), src, len);

	usize remaining = aligned_len - len;
	if (remaining) {
		std::memset(ptr_offset(this->buffer, this->offset + len), 0, remaining);
	}
	offset += aligned_len;
}

static void serializer_read_or_write(Serializer &serializer, void *data, usize len)
{
	if (serializer.is_writing) {
		serializer.write_bytes(data, len);
	} else {
		serializer.read_bytes(data, len);
	}
}

template <typename T>
void serialize_impl(Serializer &serializer, T &data)
{
	serializer_read_or_write(serializer, &data, sizeof(T));
}

void serialize(Serializer &serializer, i8 &data) { serialize_impl(serializer, data); }
void serialize(Serializer &serializer, i16 &data) { serialize_impl(serializer, data); }
void serialize(Serializer &serializer, i32 &data) { serialize_impl(serializer, data); }
void serialize(Serializer &serializer, i64 &data) { serialize_impl(serializer, data); }
void serialize(Serializer &serializer, u8 &data) { serialize_impl(serializer, data); }
void serialize(Serializer &serializer, u16 &data) { serialize_impl(serializer, data); }
void serialize(Serializer &serializer, u32 &data) { serialize_impl(serializer, data); }
void serialize(Serializer &serializer, u64 &data) { serialize_impl(serializer, data); }
void serialize(Serializer &serializer, f32 &data) { serialize_impl(serializer, data); }
void serialize(Serializer &serializer, f64 &data) { serialize_impl(serializer, data); }
void serialize(Serializer &serializer, char &data) { serialize_impl(serializer, data); }
void serialize(Serializer &serializer, bool &data) { serialize_impl(serializer, data); }

void serialize(Serializer &serializer, const char *&data)
{
	usize len = 0;
	if (serializer.is_writing) {
		len = data ? strlen(data) : 0;
		serialize(serializer, len);
		serializer.write_bytes(data, len);
	} else {
		ASSERT(serializer.scope && serializer.str_repo);

		len = 0;
		serialize(serializer, len);
		char *tmp = reinterpret_cast<char *>(serializer.scope->allocate(len + 1));
		serializer.read_bytes(tmp, len);
		tmp[len] = '\0';

		data = serializer.str_repo->intern(tmp);
	}
}

void serialize(Serializer &serializer, float4x4 &data) { return serialize_impl(serializer, data); }
void serialize(Serializer &serializer, float4 &data) { return serialize_impl(serializer, data); }
void serialize(Serializer &serializer, float2 &data) { return serialize_impl(serializer, data); }
void serialize(Serializer &serializer, int2 &data) { return serialize_impl(serializer, data); }
} // namespace exo
