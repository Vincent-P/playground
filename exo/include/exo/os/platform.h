#pragma once
#include <exo/os/prelude.h>
#include <exo/maths/numerics.h>
#include <exo/maths/vectors.h>
#include <string_view>

namespace exo
{
struct Platform;
struct Window;
struct ScopeStack;

usize platform_get_size();
Platform *platform_create(void *memory);
void platform_destroy(Platform *platform);

u64 platform_create_window(Platform *platform, int2 size, std::string_view title);
} // namespace exo
