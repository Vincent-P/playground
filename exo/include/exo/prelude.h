#pragma once
#include "exo/maths/numerics.h"
#include "exo/maths/vectors.h"
#include "exo/maths/matrices.h"
#include "exo/collections/vector.h"
#include "exo/collections/handle.h"
#include "exo/option.h"
#include "exo/result.h"
#include "exo/macros/assert.h"
#include "exo/macros/compiler.h"


/// --- Portable compiler attribute/builtins
namespace exo
{
#if defined(CXX_MSVC)
[[noreturn]] __forceinline void unreachable() {__assume(0);}
#else
[[noreturn]] inline __attribute__((always_inline)) void unreachable() {__builtin_unreachable();}
#endif
}

// --- Useful macros/functions

#define UNUSED(x) (void)(x)
#define ARRAY_SIZE(x) (sizeof(x)/sizeof(*x))

using exo::Vec;

using exo::uint2;
using exo::uint3;
using exo::int2;
using exo::int3;
using exo::float2;
using exo::float3;
using exo::float4;
using exo::float4x4;

using exo::Option;
using exo::Some;
using exo::None;

using exo::Result;
using exo::Ok;
using exo::Err;

using exo::Handle;
