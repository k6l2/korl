#pragma once
#include "korl-globalDefines.h"
#define KORL_PI32 3.14159f
#define KORL_MATH_CLAMP(x, min, max) \
    ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))
#define KORL_MATH_MIN(a,b) ((a) < (b) ? (a) : (b))
#define KORL_MATH_MAX(a,b) ((a) > (b) ? (a) : (b))
typedef union Korl_Math_V3f32
{
    struct
    {
        f32 x, y, z;
    } xyz;
    f32 elements[3];
} Korl_Math_V3f32;
const Korl_Math_V3f32 KORL_MATH_V3F32_ZERO = {.xyz = {0, 0, 0}};
const Korl_Math_V3f32 KORL_MATH_V3F32_X    = {.xyz = {1, 0, 0}};
const Korl_Math_V3f32 KORL_MATH_V3F32_Y    = {.xyz = {0, 1, 0}};
const Korl_Math_V3f32 KORL_MATH_V3F32_Z    = {.xyz = {0, 0, 1}};
typedef union Korl_Math_V4f32
{
    struct
    {
        f32 x, y, z, w;
    } xyzw;
    f32 elements[4];
} Korl_Math_V4f32;
typedef union Korl_Math_M4f32
{
    Korl_Math_V4f32 rows[4];
    f32 elements[4*4];
    struct
    {
        f32 r0c0, r0c1, r0c2, r0c3;
        f32 r1c0, r1c1, r1c2, r1c3;
        f32 r2c0, r2c1, r2c2, r2c3;
        f32 r3c0, r3c1, r3c2, r3c3;
    } rc;
} Korl_Math_M4f32;
const Korl_Math_M4f32 KORL_MATH_M4F32_IDENTITY = {.rows = 
    { {1,0,0,0}
    , {0,1,0,0}
    , {0,0,1,0}
    , {0,0,0,1} }};
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
korl_internal inline bool korl_math_isNearlyZero(f32 x);
/**  Thanks, Bruce Dawson!  Source: 
 * https://randomascii.wordpress.com/2012/02/25/comparing-floating-point-numbers-2012-edition/
 */
korl_internal inline bool korl_math_isNearlyEqualEpsilon(f32 fA, f32 fB, f32 epsilon);
/** calls korl_math_isNearlyEqualEpsilon(fA, fB, 1e-5f) */
korl_internal inline bool korl_math_isNearlyEqual(f32 fA, f32 fB);
korl_internal inline f32 korl_math_abs(f32 x);
korl_internal f32 korl_math_v3f32_magnitude(const Korl_Math_V3f32*const v);
korl_internal f32 korl_math_v3f32_magnitudeSquared(const Korl_Math_V3f32*const v);
korl_internal Korl_Math_V3f32 korl_math_v3f32_normal(Korl_Math_V3f32 v);
korl_internal Korl_Math_V3f32 korl_math_v3f32_normalKnownMagnitude(Korl_Math_V3f32 v, f32 magnitude);
korl_internal Korl_Math_V3f32 korl_math_v3f32_cross(const Korl_Math_V3f32*const vA, const Korl_Math_V3f32*const vB);
korl_internal f32 korl_math_v3f32_dot(const Korl_Math_V3f32*const vA, const Korl_Math_V3f32*const vB);
korl_internal Korl_Math_V3f32 korl_math_v3f32_subtract(Korl_Math_V3f32 vA, const Korl_Math_V3f32*const vB);
korl_internal Korl_Math_M4f32 korl_math_m4f32_transpose(const Korl_Math_M4f32*const m);
korl_internal Korl_Math_M4f32 korl_math_m4f32_projectionFov(
    f32 horizontalFovDegrees, f32 viewportWidthOverHeight, 
    f32 clipNear, f32 clipFar);
korl_internal Korl_Math_M4f32 korl_math_m4f32_lookAt(
    const Korl_Math_V3f32*const positionEye, 
    const Korl_Math_V3f32*const positionTarget, 
    const Korl_Math_V3f32*const worldUpNormal);
