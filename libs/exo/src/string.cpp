#include "exo/string.h"
#include "exo/macros/assert.h"
#include "exo/profile.h"
#include "exo/string_view.h"
#include <cstdlib>
#include <cstring>
#include <utility>

static void default_construct(exo::String &string)
{
	std::memset(string.storage.stack.buffer, 0, sizeof(string.storage.stack.buffer));
	string.storage.stack.is_small = 1;
	string.storage.stack.length   = 0;
}

static void destroy_heap_allocation(exo::String &string)
{
	ASSERT(string.storage.heap.buffer);
	std::free(string.storage.heap.buffer);
	EXO_PROFILE_MFREE(string.storage.heap.buffer);
	string.storage.heap.buffer = nullptr;
}

namespace exo
{
// -- Constructors

String::String() { default_construct(*this); }

String::~String()
{
	if (!this->storage.stack.is_small) {
		destroy_heap_allocation(*this);
	}

	default_construct(*this);
}

String::String(const char *c_string) : String{c_string, strlen(c_string)} {}

String::String(const char *c_string, usize length)
{
	// If the string + the NULL delimiter fits in the stack buffer, store it inline
	if (length + 1 <= sizeof(this->storage.stack.buffer)) {
		std::memcpy(this->storage.stack.buffer, c_string, length);
		this->storage.stack.buffer[length] = '\0';
		this->storage.stack.is_small       = 1;
		this->storage.stack.length         = (unsigned char)(length);
	} else {
		this->storage.heap.length   = u32(length);
		this->storage.heap.capacity = u32(length + 1);

		this->storage.heap.buffer = std::malloc(length + 1);
		EXO_PROFILE_MALLOC(this->storage.heap.buffer, length + 1);

		std::memcpy(this->storage.heap.buffer, c_string, length);
		static_cast<char *>(this->storage.heap.buffer)[length] = '\0';
	}
}

String::String(const StringView &string_view) : String{string_view.data(), string_view.size()} {}

String::String(const String &other) { *this = other; }

String &String::operator=(const String &other)
{
	// When we already have an allocation, we need to free our buffer in two cases:
	// - The other buffer does not have an allocation, thus we can store its content inline
	// - The other buffer has an allocation, but it has more content that our allocation can store
	if (!this->storage.stack.is_small &&
		(other.storage.stack.is_small || this->storage.heap.capacity < other.storage.heap.capacity)) {
		destroy_heap_allocation(*this);
	}

	// When the other string is allocated, we need to allocate for ourselves in two cases:
	// - We don't have an allocation, thus we need a new buffer
	// - We have an allocation but it is to small to contain the other string
	if (!other.storage.stack.is_small &&
		(this->storage.stack.is_small || this->storage.heap.capacity < other.storage.heap.capacity)) {
		this->storage.heap.buffer = std::malloc(other.storage.heap.capacity);
		EXO_PROFILE_MALLOC(this->storage.heap.buffer, other.storage.heap.capacity);
	}

	// Copy the other string into this
	if (other.storage.stack.is_small) {
		std::memcpy(this, &other, sizeof(String));
	} else {
		this->storage.heap.capacity = other.storage.heap.capacity;
		this->storage.heap.length   = other.storage.heap.length;
		std::memcpy(this->storage.heap.buffer, other.storage.heap.buffer, other.storage.heap.capacity);
	}

	return *this;
}

String::String(String &&other) { *this = std::move(other); }
String &String::operator=(String &&other)
{
	std::memcpy(this, &other, sizeof(String));
	default_construct(other);
	return *this;
}

// -- Element access

char &String::operator[](usize i)
{
	if (this->storage.stack.is_small) {
		ASSERT(i <= this->storage.stack.length);
		return this->storage.stack.buffer[i];
	} else {
		ASSERT(i <= this->storage.heap.length);
		return static_cast<char *>(this->storage.heap.buffer)[i];
	}
}

const char &String::operator[](usize i) const
{
	if (this->storage.stack.is_small) {
		ASSERT(i <= this->storage.stack.length);
		return this->storage.stack.buffer[i];
	} else {
		ASSERT(i <= this->storage.heap.length);
		return static_cast<char *>(this->storage.heap.buffer)[i];
	}
}

char &String::back()
{
	if (this->storage.stack.is_small) {
		ASSERT(this->storage.stack.length - 1u < String::SSBO_CAPACITY);
		return this->storage.stack.buffer[this->storage.stack.length - 1u];
	} else {
		ASSERT(this->storage.heap.length < this->storage.heap.capacity);
		return static_cast<char *>(this->storage.heap.buffer)[this->storage.heap.length - 1u];
	}
}

// -- Observers

// -- Operations

void String::push_back(char c)
{
	if (this->storage.stack.is_small) {
		if (this->storage.stack.length + 2u < String::SSBO_CAPACITY) {
			this->storage.stack.buffer[this->storage.stack.length++] = c;
			this->storage.stack.buffer[this->storage.stack.length]   = '\0';
		} else {
			u32 new_capacity = String::SSBO_CAPACITY * 2;
			u32 new_length   = this->storage.stack.length + 1;

			auto *new_buffer = static_cast<char *>(std::malloc(new_capacity));
			EXO_PROFILE_MALLOC(new_buffer, new_capacity);

			std::memcpy(new_buffer, this->storage.stack.buffer, this->storage.stack.length);
			new_buffer[new_length - 1u] = c;
			new_buffer[new_length]      = '\0';

			this->storage.heap.capacity = new_capacity;
			this->storage.heap.length   = new_length;
			this->storage.heap.buffer   = new_buffer;
		}
	} else {
		if (this->storage.heap.length + 2u > this->storage.heap.capacity) {
			auto new_capacity = 2u * this->storage.heap.capacity;
			ASSERT(new_capacity);
			auto *new_buffer = std::realloc(this->storage.heap.buffer, new_capacity);
			EXO_PROFILE_MFREE(this->storage.heap.buffer);
			EXO_PROFILE_MALLOC(new_buffer, new_capacity);

			this->storage.heap.capacity = new_capacity;
			this->storage.heap.buffer   = new_buffer;
		}

		char *chars = static_cast<char *>(this->storage.heap.buffer);

		chars[this->storage.heap.length++] = c;
		chars[this->storage.heap.length]   = '\0';
	}
}

void String::clear()
{
	if (this->storage.stack.is_small) {
		this->storage.stack.length    = 0;
		this->storage.stack.buffer[0] = '\0';
	} else {
		this->storage.heap.length                         = 0;
		static_cast<char *>(this->storage.heap.buffer)[0] = '\0';
	}
}

void String::reserve(usize new_capacity)
{
	if (new_capacity <= this->capacity()) {
		return;
	}

	ASSERT(new_capacity > SSBO_CAPACITY);

	void *new_buffer = std::malloc(new_capacity);
	EXO_PROFILE_MALLOC(new_buffer, new_capacity);

	if (this->storage.stack.is_small) {
		std::memcpy(new_buffer, this->storage.stack.buffer, this->storage.stack.length);
		static_cast<char *>(new_buffer)[this->storage.stack.length] = '\0';
		this->storage.heap.length                                   = this->storage.stack.length;
	} else {
		std::memcpy(new_buffer, this->storage.heap.buffer, this->storage.heap.length);
		static_cast<char *>(new_buffer)[this->storage.heap.length] = '\0';

		destroy_heap_allocation(*this);
	}

	this->storage.heap.buffer   = new_buffer;
	this->storage.heap.capacity = u32(new_capacity);
}

void String::resize(usize new_length)
{
	usize cur_len = this->size();
	usize cur_cap = this->capacity();

	if (new_length > cur_cap) {
		this->reserve(new_length + 1);
	}

	if (new_length > cur_len) {
		for (usize i = cur_len; i < new_length; ++i) {
			this->push_back('\0');
		}
	} else {
		if (this->storage.stack.is_small) {
			this->storage.stack.length                             = (unsigned char)(new_length);
			this->storage.stack.buffer[this->storage.stack.length] = '\0';
		} else {
			this->storage.heap.length                                                 = u32(new_length);
			static_cast<char *>(this->storage.heap.buffer)[this->storage.heap.length] = '\0';
		}
	}
}

bool operator==(const String &lhs, const String &rhs)
{
	return lhs.size() == rhs.size() && std::memcmp(lhs.data(), rhs.data(), lhs.size()) == 0;
}

String operator+(const StringView &lhs, const StringView &rhs)
{
	auto lhs_size = lhs.size();
	auto rhs_size = rhs.size();

	const char *lhs_data = lhs.data();
	const char *rhs_data = rhs.data();

	String result;
	result.reserve(lhs_size + rhs_size + 1);

	char *res_data = result.data();
	std::memcpy(res_data, lhs_data, lhs_size);
	std::memcpy(res_data + lhs_size, rhs_data, rhs_size);
	res_data[lhs_size + rhs_size] = '\0';

	if (result.storage.stack.is_small) {
		result.storage.stack.length = (unsigned char)(lhs_size + rhs_size);
	} else {
		result.storage.heap.length = u32(lhs_size + rhs_size);
	}

	return result;
}
} // namespace exo
