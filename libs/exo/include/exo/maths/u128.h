#pragma once
#include <exo/maths/numerics.h>

#include <emmintrin.h>
#include <smmintrin.h>

namespace exo
{
using u128 = __m128i;

#if __x86_64__ || _M_AMD64
inline void u128_to_u64(u128 value, u64 *val0, u64 *val1)
{
	*val0 = _mm_extract_epi64(value, 0);
	*val1 = _mm_extract_epi64(value, 1);
}

inline u128 u128_from_u64(u64 val1, u64 val0) { return _mm_set_epi64x(val1, val0); }
#else
#error Cannot determine architecture to use!
#endif

} // namespace exo
