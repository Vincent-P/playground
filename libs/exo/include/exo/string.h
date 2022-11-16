#pragma once
#include "exo/maths/numerics.h"

namespace exo
{
struct StringView;

struct String
{
	struct HeapString
	{
		u32   capacity;
		u32   length;
		void *buffer;
	};

	inline static constexpr usize SSBO_CAPACITY = sizeof(HeapString) - 1;

	struct StackString
	{
		char          buffer[SSBO_CAPACITY];
		unsigned char is_small : 1;
		unsigned char length : 7;
	};

	static_assert(sizeof(HeapString) == sizeof(StackString));
	inline static constexpr StackString EMPTY_STACK_STRING = StackString{.buffer = {}, .is_small = 1, .length = 0};

	union Storage
	{
		HeapString  heap;
		StackString stack;
	} storage = {.stack = EMPTY_STACK_STRING};
	// --

	String();
	~String();

	String(const char *c_string);
	String(const char *c_string, usize length);
	String(const StringView &string_view);

	String(const String &other);
	String &operator=(const String &other);

	String(String &&other) noexcept;
	String &operator=(String &&other) noexcept;

	// -- Element access

	char       &operator[](usize i);
	const char &operator[](usize i) const;

	// -- Observers

	bool is_heap_allocated() const { return !this->storage.stack.is_small; }

	const char *c_str() const { return this->data(); }

	usize capacity() const
	{
		return this->storage.stack.is_small ? sizeof(this->storage.stack.buffer) : this->storage.heap.capacity;
	}

	bool is_empty() const { return this->size() == 0; }

	// -- STL compat

	void clear();
	void reserve(usize new_capacity);
	void resize(usize new_length);

	usize size() const { return this->storage.stack.is_small ? this->storage.stack.length : this->storage.heap.length; }
	bool  empty() const { return this->size() == 0; }
	char &back();
	void  push_back(char c);
	char *data()
	{
		return this->storage.stack.is_small ? this->storage.stack.buffer
		                                    : static_cast<char *>(this->storage.heap.buffer);
	}
	const char *data() const
	{
		return this->storage.stack.is_small ? this->storage.stack.buffer
		                                    : static_cast<const char *>(this->storage.heap.buffer);
	}
};

bool   operator==(const String &lhs, const String &rhs);
String operator+(const StringView &lhs, const StringView &rhs);

template <usize N>
inline bool operator==(const String &string, const char (&literal)[N])
{
	// N includes the NULL terminator
	if (string.size() != N - 1) {
		return false;
	}

	const char *data = string.data();
	for (usize i = 0; i < N - 1; ++i) {
		if (data[i] != literal[i]) {
			return false;
		}
	}

	return true;
}

template <usize N>
inline bool operator==(const char (&literal)[N], const String &view)
{
	return view == literal;
}

} // namespace exo
