#pragma once
#include <Tracy.hpp>

namespace exo::profile
{
inline constexpr unsigned MAX_CALLSTACK_DEPTH = 12;
}

#define EXO_PROFILE_FRAMEMARK FrameMark
#define EXO_PROFILE_SCOPE ZoneScoped
#define EXO_PROFILE_SCOPE_NAMED(name) ZoneScopedN(name)

#define EXO_PROFILE_MALLOC(ptr, size) TracyAllocS(ptr, size, ::exo::profile::MAX_CALLSTACK_DEPTH)
#define EXO_PROFILE_MFREE(ptr) TracyFreeS(ptr, ::exo::profile::MAX_CALLSTACK_DEPTH)
