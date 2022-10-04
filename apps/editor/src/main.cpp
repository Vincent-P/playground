#include "app.h"

#include <exo/memory/string_repository.h>
#include <exo/memory/linear_allocator.h>
#include <exo/memory/scope_stack.h>
#include <exo/profile.h>

void *operator new(std::size_t count)
{
	auto ptr = malloc(count);
	EXO_PROFILE_MALLOC(ptr, count);
	return ptr;
}

void operator delete(void *ptr) noexcept
{
	EXO_PROFILE_MFREE(ptr);
	free(ptr);
}

int main(int /*argc*/, char ** /*argv*/)
{
	constexpr usize global_stack_size = 4_MiB;
	u8             *global_stack_mem  = (u8 *)calloc(1, global_stack_size);

	exo::tls_string_repository = new exo::StringRepository;
	*exo::tls_string_repository = exo::StringRepository::create();

	exo::LinearAllocator global_allocator =
		exo::LinearAllocator::with_external_memory(global_stack_mem, global_stack_size);
	exo::ScopeStack global_scope = exo::ScopeStack::with_allocator(&global_allocator);

	auto *app = App::create(global_scope);
	app->run();

	return 0;
}
