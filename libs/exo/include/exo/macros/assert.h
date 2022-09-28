#pragma once
void internal_assert(bool condition, const char *condition_str);

#define STR(x) #x
#define ASSERT(x) internal_assert(x, STR(x))
