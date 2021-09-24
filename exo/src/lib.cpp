#include "exo/prelude.h"

#include <cstdio>
#include <cstdlib>

#if !defined(__PRETTY_FUNCTION__)
#define __PRETTY_FUNCTION__ __FUNCTION__
#endif

void internal_assert(bool condition, const char *condition_str)
{
    if (!condition)
    {
        fprintf(stderr, "My custom assertion failed: (%s), function %s, file %s, line %d.\n", condition_str, __PRETTY_FUNCTION__, __FILE__, __LINE__);
        abort();
    }
}
