#include "exo/string_view.h"
#include "exo/macros/assert.h"
#include "exo/string.h"
#include <cstring>
#include <utility>
#include <xxhash.h>

namespace exo
{

// -- Constructors

StringView::StringView(const char *c_string) : StringView{c_string, strlen(c_string)} {}
StringView::StringView(const char *c_string, usize len) : ptr{c_string}, length{len} {}

StringView::StringView(const String &string) : ptr{string.c_str()}, length{string.len()} {}

StringView::StringView(StringView &&other) noexcept { *this = std::move(other); }
StringView &StringView::operator=(StringView &&other) noexcept
{
	this->ptr = other.ptr;
	this->length = other.length;
	other.ptr = nullptr;
	other.length = 0;
	return *this;
}

// -- Element access

const char &StringView::operator[](usize i) const
{
	ASSERT(i < this->length);
	return this->ptr[i];
}

// -- Observers

bool operator==(const StringView &lhs, const StringView &rhs)
{
	return lhs.length == rhs.length && std::memcmp(lhs.ptr, rhs.ptr, lhs.length) == 0;
}

bool operator==(const String &lhs, const StringView &rhs)
{
	return lhs.len() == rhs.length && std::memcmp(lhs.data(), rhs.ptr, rhs.length) == 0;
}

bool operator==(const StringView &lhs, const String &rhs)
{
	return lhs.length == rhs.len() && std::memcmp(lhs.ptr, rhs.data(), lhs.length) == 0;
}

[[nodiscard]] u64 hash_value(const exo::StringView view)
{
	const u64 hash = XXH3_64bits(view.data(), view.len());
	return hash;
}

}; // namespace exo
