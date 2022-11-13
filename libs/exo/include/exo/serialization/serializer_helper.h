#pragma once
#include <exo/memory/scope_stack.h>
#include <exo/profile.h>
#include <exo/serialization/serializer.h>

#include <cstdio>
#include "exo/collections/span.h"
#include <string_view>

namespace exo::serializer_helper
{
template <typename T>
static void read_object(exo::Span<const u8> data, T &object)
{
	exo::ScopeStack scope = exo::ScopeStack::with_allocator(&exo::tls_allocator);

	auto serializer        = exo::Serializer::create(&scope);
	serializer.buffer_size = data.size_bytes();
	// const_cast, this pointer should always be READ if is_writing == false
	serializer.buffer     = const_cast<u8 *>(data.data());
	serializer.is_writing = false;
	serialize(serializer, object);
}

template <typename T>
static void write_object_to_file(std::string_view output_path, T &object)
{
	exo::ScopeStack scope      = exo::ScopeStack::with_allocator(&exo::tls_allocator);
	exo::Serializer serializer = exo::Serializer::create(&scope);
	serializer.buffer_size     = 96_MiB;
	serializer.buffer          = malloc(serializer.buffer_size);
	serializer.is_writing      = true;

	EXO_PROFILE_MALLOC(serializer.buffer, serializer.buffer_size);

	serialize(serializer, object);

	FILE *fp       = fopen(output_path.data(), "wb"); // non-Windows use "w"
	auto  bwritten = fwrite(serializer.buffer, 1, serializer.offset, fp);
	ASSERT(bwritten == serializer.offset);
	fclose(fp);

	EXO_PROFILE_MFREE(serializer.buffer);
	free(serializer.buffer);
}
} // namespace exo::serializer_helper
