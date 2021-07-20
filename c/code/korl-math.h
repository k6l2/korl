#pragma once
#include "korl-globalDefines.h"
#define KORL_MATH_CLAMP(x, min, max) \
    ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))
korl_internal inline u64 korl_math_kilobytes(u64 x);
korl_internal inline u64 korl_math_megabytes(u64 x);
korl_internal inline uintptr_t korl_math_roundUpPowerOf2(
    uintptr_t value, uintptr_t powerOf2Multiple);
