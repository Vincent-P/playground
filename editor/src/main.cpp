#include "app.h"

#include <Tracy.hpp>
#include <exo/memory/linear_allocator.h>
#include <exo/memory/scope_stack.h>

#if defined(ENABLE_DOCTEST)
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest.h>
#endif

u8 global_stack_mem[64 << 10];

void *operator new(std::size_t count)
{
	auto ptr = malloc(count);
	TracyAlloc(ptr, count);
	return ptr;
}

void operator delete(void *ptr) noexcept
{
	TracyFree(ptr);
	free(ptr);
}

int main(int argc, char **argv)
{
#if defined(ENABLE_DOCTEST)
	doctest::Context context;
	context.applyCommandLine(argc, argv);
	int res = context.run(); // run
	if (context.shouldExit()) {
		return res;
	}
#else
#endif

	exo::LinearAllocator global_allocator =
		exo::LinearAllocator::with_external_memory(global_stack_mem, sizeof(global_stack_mem));
	exo::ScopeStack global_scope = exo::ScopeStack::with_allocator(&global_allocator);

	auto *app = App::create(global_scope);
	app->run();

	return 0;
}
