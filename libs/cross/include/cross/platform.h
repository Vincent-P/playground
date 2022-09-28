#pragma once
#include <exo/maths/numerics.h>

namespace cross::platform
{
struct Platform;
static Platform *g_platform = nullptr;

usize get_size();
void  create(void *memory);
void  destroy();
void *win32_get_main_fiber();
} // namespace cross::platform
