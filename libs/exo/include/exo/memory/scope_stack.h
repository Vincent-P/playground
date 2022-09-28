#pragma once
#include "exo/macros/assert.h"
#include "exo/maths/pointer.h"
#include "exo/memory/linear_allocator.h"

#include <type_traits>

namespace exo
{
struct Finalizer
{
	void (*fn)(void *ptr);
	Finalizer *chain;
};

struct ScopeStack
{
public:
	static ScopeStack with_allocator(LinearAllocator *a);
	~ScopeStack();

	template <typename T> T *allocate(u32 element_count = 1);

	inline void *allocate(usize size);

	ScopeStack()                        = default;
	ScopeStack(const ScopeStack &other) = delete;
	ScopeStack &operator=(const ScopeStack &other) = delete;
	ScopeStack(ScopeStack &&other) noexcept;
	ScopeStack &operator=(ScopeStack &&other) noexcept;

private:
	static inline void *object_from_finalizer(Finalizer *f)
	{
		return reinterpret_cast<u8 *>(f) + round_up_to_alignment(sizeof(u32), sizeof(Finalizer));
	}

	LinearAllocator *allocator      = nullptr;
	void            *rewind_ptr     = nullptr;
	Finalizer       *finalizer_head = nullptr;
};

template <typename T> void call_dtor(void *ptr) { static_cast<T *>(ptr)->~T(); }

template <typename T> T *ScopeStack::allocate(u32 element_count)
{
	if constexpr (!std::is_trivially_destructible_v<T>) {
		// Not sure how to implement multiple elements
		ASSERT(element_count == 1);

		// Allocate enough space for the finalizer + the object itself
		const usize total_size = sizeof(T) + round_up_to_alignment(sizeof(u32), sizeof(Finalizer));
		auto       *f          = reinterpret_cast<Finalizer *>(allocator->allocate(total_size));

		// Call the constructor with placement new
		T *result = new (object_from_finalizer(f)) T;

		// Push the finalizer to the top of the list
		f->fn                = &call_dtor<T>;
		f->chain             = this->finalizer_head;
		this->finalizer_head = f;
		return result;
	} else {
		// Allocate memory for T, call its constructor
		T *memory = reinterpret_cast<T *>(allocator->allocate(element_count * sizeof(T)));

		for (u32 i_element = 0; i_element < element_count; ++i_element) {
			new (memory + i_element) T;
		}

		return memory;
	}
}

inline void *ScopeStack::allocate(usize size) { return allocator->allocate(size); }
} // namespace exo
