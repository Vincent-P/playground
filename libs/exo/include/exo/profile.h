#pragma once

#define EXO_PROFILE_USE_TRACY

#if defined(EXO_PROFILE_USE_TRACY)

#include <Tracy.hpp>

namespace exo::profile
{
inline constexpr unsigned MAX_CALLSTACK_DEPTH = 8;
}

#define EXO_PROFILE_FRAMEMARK FrameMark
#define EXO_PROFILE_SCOPE ZoneScoped
#define EXO_PROFILE_SCOPE_NAMED(name) ZoneScopedN(name)

#define EXO_PROFILE_PLOT_VALUE(name, value) TracyPlot(name, value)

#define EXO_PROFILE_MALLOC(ptr, size) TracyAllocS(ptr, size, ::exo::profile::MAX_CALLSTACK_DEPTH)
#define EXO_PROFILE_MFREE(ptr) TracyFreeS(ptr, ::exo::profile::MAX_CALLSTACK_DEPTH)

#define EXO_PROFILE_SWITCH_TO_FIBER(name) TracyFiberEnter(name)
#define EXO_PROFILE_LEAVE_FIBER TracyFiberLeave

#else

#define EXO_PROFILE_FRAMEMARK
#define EXO_PROFILE_SCOPE
#define EXO_PROFILE_SCOPE_NAMED(name)

#define EXO_PROFILE_PLOT_VALUE(name, value)

#define EXO_PROFILE_MALLOC(ptr, size)
#define EXO_PROFILE_MFREE(ptr)

#define EXO_PROFILE_SWITCH_TO_FIBER(name)
#define EXO_PROFILE_LEAVE_FIBER

#endif
