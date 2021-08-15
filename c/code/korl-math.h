#pragma once
#include "korl-globalDefines.h"
#define KORL_MATH_CLAMP(x, min, max) \
    ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))
typedef union Korl_Math_V3f32
{
    struct
    {
        f32 x, y, z;
    } xyz;
    f32 elements[3];
} Korl_Math_V3f32;
typedef union Korl_Math_V3u8
{
    struct
    {
        u8 x, y, z;
    } xyz;
    struct
    {
        u8 r, g, b;
    } rgb;
    u8 elements[3];
} Korl_Math_V3u8;
korl_internal inline u64 korl_math_kilobytes(u64 x);
korl_internal inline u64 korl_math_megabytes(u64 x);
korl_internal inline uintptr_t korl_math_roundUpPowerOf2(
    uintptr_t value, uintptr_t powerOf2Multiple);
