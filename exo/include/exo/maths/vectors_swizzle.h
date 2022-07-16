/*
  These recursive macros enumerate all possible swizzles (combination of components) of vectors.
  each SN macro enumerate all combination of N components.

  for example to enumerate all combinations of 2 components of a 4 components vector:
  #include "vectors_swizzle.h"
  #define VEC4_S2(a, b) vec2 a##b() { return {a, b}; }
  VEC4_SWIZZLES

  will generate this:
    vec2 xx() { return {x, x}; }
    vec2 xy() { return {x, y}; }
    vec2 xz() { return {x, z}; }
    ...
    vec2 wz() { return {w, z}; }
    vec2 ww() { return {w, w}; }
 */

#undef VEC4_S3
#undef VEC4_S2
#undef VEC4_S1
#undef VEC4_SWIZZLES
#undef VEC3_S2
#undef VEC3_S1
#undef VEC3_SWIZZLES
#undef VEC2_S1
#undef VEC2_SWIZZLES

#define VEC4_S3(a, b, c) VEC4_S4(a, b, c, x) VEC4_S4(a, b, c, y) VEC4_S4(a, b, c, z) VEC4_S4(a, b, c, w)
#define VEC4_S2(a, b) VEC4_S3(a, b, x) VEC4_S3(a, b, y) VEC4_S3(a, b, z) VEC4_S3(a, b, w)
#define VEC4_S1(a) VEC4_S2(a, x) VEC4_S2(a, y) VEC4_S2(a, z) VEC4_S2(a, w)
#define VEC4_SWIZZLES VEC4_S1(x) VEC4_S1(y) VEC4_S1(z) VEC4_S1(w)

#define VEC3_S2(a, b) VEC3_S3(a, b, x) VEC3_S3(a, b, y) VEC3_S3(a, b, z)
#define VEC3_S1(a) VEC3_S2(a, x) VEC3_S2(a, y) VEC3_S2(a, z)
#define VEC3_SWIZZLES VEC3_S1(x) VEC3_S1(y) VEC3_S1(z)

#define VEC2_S1(a) VEC2_S2(a, x) VEC2_S2(a, y)
#define VEC2_SWIZZLES VEC2_S1(x) VEC2_S1(y)
