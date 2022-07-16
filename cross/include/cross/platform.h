#pragma once
#include <exo/maths/numerics.h>
#include <exo/maths/vectors.h>
#include <string_view>

namespace exo
{
struct ScopeStack;
}

namespace cross
{
struct Platform;
struct Window;

usize     platform_get_size();
Platform *platform_create(void *memory);
void      platform_destroy(Platform *platform);

void *platform_win32_get_main_fiber(Platform *platform);
} // namespace cross
