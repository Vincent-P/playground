// -*- mode: glsl; -*-

#ifndef SEQUENCES_H
#define SEQUENCES_H

#include "types.h"

float r1_sequence(uint n)
{
    float g = 1.6180339887498948482;
    float a1 = 1.0/g;
    return fract(0.5+a1*n);
}

float2 r2_sequence(uint n)
{
    float g = 1.32471795724474602596;
    float a1 = 1.0/g;
    float2 a = float2(a1, a1*a1);
    return fract(float2(0.5) + a * n);
}

float2 apply_r2_sequence(float2 x, uint n)
{
    float g = 1.32471795724474602596;
    float a1 = 1.0/g;
    float2 a = float2(a1, a1*a1);
    return fract(x + a * n);
}

#endif
