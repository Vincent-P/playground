#pragma once
#include <exo/memory/scope_stack.h>
#include <exo/serializer.h>

#include <cstdio>
#include <span>

namespace exo::serializer_helper
{
template <typename T> static void read_object(std::span<const u8> data, T &object)
{
	exo::ScopeStack scope = exo::ScopeStack::with_allocator(&exo::tls_allocator);

	auto serializer        = exo::Serializer::create(&scope);
	serializer.buffer_size = data.size_bytes();
	// const_cast, this pointer should always be READ if is_writing == false
	serializer.buffer     = const_cast<u8 *>(data.data());
	serializer.is_writing = false;
	serialize(serializer, object);
}

template <typename T> static void write_object_to_file(std::string_view output_path, T &object)
{
	exo::ScopeStack scope = exo::ScopeStack::with_allocator(&exo::tls_allocator);

	exo::Serializer serializer = exo::Serializer::create(&scope);
	serializer.buffer_size     = 96_MiB;
	serializer.buffer          = scope.allocate(serializer.buffer_size);
	serializer.is_writing      = true;

	serialize(serializer, object);

	FILE *fp       = fopen(output_path.data(), "wb"); // non-Windows use "w"
	auto  bwritten = fwrite(serializer.buffer, 1, serializer.offset, fp);
	ASSERT(bwritten == serializer.offset);
	fclose(fp);
}
} // namespace exo::serializer_helper