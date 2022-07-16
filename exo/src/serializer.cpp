#include "exo/serializer.h"

#include "exo/maths/matrices.h"
#include "exo/maths/vectors.h"
#include "exo/memory/scope_stack.h"
#include "exo/memory/string_repository.h"

namespace exo
{
Serializer Serializer::create(ScopeStack *s, StringRepository *r)
{
	Serializer result  = {};
	result.str_repo    = r ? r : &tls_string_repository;
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

static void serializer_read_or_write(Serializer *serializer, void *data, usize len)
{
	if (serializer->is_writing) {
		serializer->write_bytes(data, len);
	} else {
		serializer->read_bytes(data, len);
	}
}

template <typename T> void serialize_impl(Serializer *serializer, T &data)
{
	serializer_read_or_write(serializer, &data, sizeof(T));
}

void Serializer::serialize(i8 &data) { serialize_impl(this, data); }
void Serializer::serialize(i16 &data) { serialize_impl(this, data); }
void Serializer::serialize(i32 &data) { serialize_impl(this, data); }
void Serializer::serialize(i64 &data) { serialize_impl(this, data); }
void Serializer::serialize(u8 &data) { serialize_impl(this, data); }
void Serializer::serialize(u16 &data) { serialize_impl(this, data); }
void Serializer::serialize(u32 &data) { serialize_impl(this, data); }
void Serializer::serialize(u64 &data) { serialize_impl(this, data); }
void Serializer::serialize(f32 &data) { serialize_impl(this, data); }
void Serializer::serialize(f64 &data) { serialize_impl(this, data); }
void Serializer::serialize(char &data) { serialize_impl(this, data); }
void Serializer::serialize(bool &data) { serialize_impl(this, data); }

void Serializer::serialize(const char *&data)
{
	usize len = 0;
	if (this->is_writing) {
		len = data ? strlen(data) : 0;
		this->serialize(len);
		this->write_bytes(data, len);
	} else {
		ASSERT(this->scope && this->str_repo);

		len = 0;
		this->serialize(len);
		char *tmp = reinterpret_cast<char *>(this->scope->allocate(len + 1));
		this->read_bytes(tmp, len);
		tmp[len] = '\0';

		data = this->str_repo->intern(tmp);
	}
}

void Serializer::serialize(float4x4 &data) { return serialize_impl(this, data); }
void Serializer::serialize(float4 &data) { return serialize_impl(this, data); }
void Serializer::serialize(float2 &data) { return serialize_impl(this, data); }
void Serializer::serialize(int2 &data) { return serialize_impl(this, data); }
} // namespace exo
