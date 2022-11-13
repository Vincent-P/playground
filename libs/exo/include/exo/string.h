#pragma once
#include "exo/maths/numerics.h"

namespace exo
{
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

	union Storage
	{
		HeapString  heap;
		StackString stack;
	} storage;
	// --

	String();
	~String();

	String(const char *c_string);
	String(const char *c_string, usize length);

	String(const String &other);
	String &operator=(const String &other);

	String(String &&other);
	String &operator=(String &&other);

	// -- Element access

	char       &operator[](usize i);
	const char &operator[](usize i) const;

	// -- Observers

	bool is_heap_allocated() const { return !this->storage.stack.is_small; }

	const char *c_str() const
	{
		return this->storage.stack.is_small ? this->storage.stack.buffer
		                                    : static_cast<const char *>(this->storage.heap.buffer);
	}

	usize capacity() const
	{
		return this->storage.stack.is_small ? sizeof(this->storage.stack.buffer) : this->storage.heap.capacity;
	}

	bool is_empty() const { return this->size() == 0; }

	// -- STL compat
	usize size() const { return this->storage.stack.is_small ? this->storage.stack.length : this->storage.heap.length; }
	bool  empty() const { return this->size() == 0; }
	char &back();
	void  push_back(char c);
};
} // namespace exo
