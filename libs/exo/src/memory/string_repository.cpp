#include "exo/memory/string_repository.h"

#include "exo/maths/pointer.h"
#include "exo/memory/virtual_allocator.h"

#include <cstring>
#include <xxhash.h>

namespace exo
{
StringRepository StringRepository::create() { return StringRepository::with_capacity(1_GiB); }

StringRepository StringRepository::with_capacity(usize capacity)
{
	StringRepository repository = {};
	repository.offsets = {};
	repository.string_buffer = reinterpret_cast<char *>(virtual_allocator::reserve(capacity));
	return repository;
}

StringRepository::~StringRepository() { virtual_allocator::free(this->string_buffer); }

StringRepository::StringRepository(StringRepository &&other) noexcept { *this = std::move(other); }

StringRepository &StringRepository::operator=(StringRepository &&other) noexcept
{
	this->offsets = std::move(other.offsets);
	this->string_buffer = std::exchange(other.string_buffer, nullptr);
	this->buffer_size = std::exchange(other.buffer_size, 0);
	return *this;
}

const char *StringRepository::intern(exo::StringView s)
{
	const auto hash = exo::RawHash{XXH3_64bits(s.data(), s.len())};

	// If the string is already interned, return its offset
	if (const auto *offset = offsets.at(hash)) {
		return this->string_buffer + *offset;
	}

	// Commit more memory if needed
	const usize page_size = virtual_allocator::get_page_size();
	const usize old_size = this->buffer_size;
	const usize new_size = this->buffer_size + s.len() + 1;
	const usize page_count = round_up_to_alignment(page_size, old_size);
	const usize new_page_count = round_up_to_alignment(page_size, new_size);
	if (new_page_count != page_count) {
		virtual_allocator::commit(this->string_buffer + page_count * page_size,
			(new_page_count - page_count) * page_size);
	}

	std::memcpy(string_buffer + this->buffer_size, s.data(), s.len() + 1);
	offsets.insert(hash, this->buffer_size);
	this->buffer_size = new_size;

	return this->string_buffer + old_size;
}

bool StringRepository::is_interned(exo::StringView s)
{
	const auto hash = exo::RawHash{XXH3_64bits(s.data(), s.len())};
	return offsets.at(hash) != nullptr;
}
} // namespace exo
