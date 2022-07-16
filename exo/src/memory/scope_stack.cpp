#include "exo/memory/scope_stack.h"
#include "exo/logger.h"

#include <utility>

namespace exo
{
ScopeStack ScopeStack::with_allocator(LinearAllocator *a)
{
	ScopeStack result     = {};
	result.allocator      = a;
	result.rewind_ptr     = a->get_ptr();
	result.finalizer_head = nullptr;
	return result;
}

ScopeStack::~ScopeStack()
{
	for (auto *f = this->finalizer_head; f != nullptr; f = f->chain) {
		(*f->fn)(object_from_finalizer(f));
	}

	// If the scope stack was moved, all fields are set to nullptr
	if (this->allocator) {
		this->allocator->rewind(this->rewind_ptr);
	}
}

ScopeStack::ScopeStack(ScopeStack &&other) { *this = std::move(other); }

ScopeStack &ScopeStack::operator=(ScopeStack &&other)
{
	this->allocator      = std::exchange(other.allocator, nullptr);
	this->rewind_ptr     = std::exchange(other.rewind_ptr, nullptr);
	this->finalizer_head = std::exchange(finalizer_head, nullptr);
	return *this;
}
} // namespace exo
