#include "korl-math.h"
#include "korl-checkCast.h"
#include <math.h>
#ifndef KORL_MATH_ASSERT
    #define KORL_MATH_ASSERT(condition) korl_assert(condition)
#endif
#ifndef KORL_MATH_ZERO_STACK
    #define KORL_MATH_ZERO_STACK(variableType, variableIdentifier) \
        KORL_ZERO_STACK(variableType, variableIdentifier)
#endif
korl_internal inline u64 korl_math_kilobytes(u64 x)
{
    return 1024 * x;
}
korl_internal inline u64 korl_math_megabytes(u64 x)
{
    return 1024 * korl_math_kilobytes(x);
}
korl_internal inline u64 korl_math_gigabytes(u64 x)
{
    return 1024 * korl_math_megabytes(x);
}
korl_internal inline u32 korl_math_round_f32_to_u32(f32 x)
{
    /** maybe in the future, we can have a general solution? 
     * https://stackoverflow.com/a/497079 */
    x = roundf(x);
    KORL_MATH_ASSERT(x >= 0.0f);
    KORL_MATH_ASSERT(x <= (f32)0xFFFFFFFF);
    return KORL_C_CAST(u32, x);
}
korl_internal inline u$ korl_math_nextHighestDivision(u$ value, u$ division)
{
    KORL_MATH_ASSERT(division);// it doesn't make sense to divide by 0
    return (value + (division - 1)) / division;
}
korl_internal inline u$ korl_math_roundUpPowerOf2(u$ value, u$ powerOf2Multiple)
{
    /* derived from this source: https://stackoverflow.com/a/9194117 */
    KORL_MATH_ASSERT(powerOf2Multiple);                                // ensure multiple is non-zero
    KORL_MATH_ASSERT((powerOf2Multiple & (powerOf2Multiple - 1)) == 0);// ensure multiple is a power of two
    return (value + (powerOf2Multiple - 1)) & ~(powerOf2Multiple - 1);
}
korl_internal inline bool korl_math_isNearlyZero(f32 x)
{
    return korl_math_isNearlyEqual(x, 0.f);
}
korl_internal inline bool korl_math_isNearlyEqualEpsilon(f32 fA, f32 fB, f32 epsilon)
{
    const f32 diff = korl_math_abs(fA - fB);
    fA = korl_math_abs(fA);
    fB = korl_math_abs(fB);
    const f32 largest = (fB > fA) ? fB : fA;
    return (diff <= largest * epsilon);
}
korl_internal inline bool korl_math_isNearlyEqual(f32 fA, f32 fB)
{
    return korl_math_isNearlyEqualEpsilon(fA, fB, 1e-5f);
}
korl_internal inline f32 korl_math_abs(f32 x)
{
    if(x >= 0)
        return x;
    return -x;
}
korl_internal inline f32 korl_math_fmod(f32 numerator, f32 denominator)
{
    return fmodf(numerator, denominator);
}
korl_internal inline f32 korl_math_acos(f32 x)
{
    return acosf(x);
}
korl_internal inline f32 korl_math_ceil(f32 x)
{
    return ceilf(x);
}
korl_internal inline f32 korl_math_sqrt(f32 x)
{
    return sqrtf(x);
}
korl_internal inline f32 korl_math_nanf32(void)
{
    korl_shared_const u32 MAX_U32 = KORL_U32_MAX;
    return *KORL_C_CAST(f32*, &MAX_U32);
}
korl_internal inline bool korl_math_isNanf32(f32 x)
{
    return 0 != _isnanf(x);
}
korl_internal inline Korl_Math_Rng_WichmannHill korl_math_rng_wichmannHill_new(u16 seed0, u16 seed1, u16 seed2)
{
    KORL_ZERO_STACK(Korl_Math_Rng_WichmannHill, result);
    /** seed parameters should be in the range [1, 30_000] */
    result.seed[0] = korl_checkCast_i$_to_u16((seed0 % 30000) + 1);
    result.seed[1] = korl_checkCast_i$_to_u16((seed1 % 30000) + 1);
    result.seed[2] = korl_checkCast_i$_to_u16((seed2 % 30000) + 1);
    return result;
}
korl_internal inline Korl_Math_Rng_WichmannHill korl_math_rng_wichmannHill_new_u64(u64 seed)
{
    KORL_ZERO_STACK(Korl_Math_Rng_WichmannHill, result);
    /* break the provided seed into parts by doing the following:
        - take the bottom 16 bits
        - wrap to the range [0, 29999]
        - +1 to move to the range [1, 30000], which is the desired seed range */
    result.seed[0] = (( seed        & 0xFFFF) % 30000) + 1;
    result.seed[1] = (((seed >> 16) & 0xFFFF) % 30000) + 1;
    result.seed[2] = (((seed >> 32) & 0xFFFF) % 30000) + 1;
    return result;
}
korl_internal inline f32 korl_math_rng_wichmannHill_f32_0_1(Korl_Math_Rng_WichmannHill*const context)
{
    korl_shared_const u16 WICHMANN_HILL_CONSTS[] = { 30269, 30307, 30323 };
    context->seed[0] = korl_checkCast_i$_to_u16((171 * context->seed[0]) % WICHMANN_HILL_CONSTS[0]);
    context->seed[1] = korl_checkCast_i$_to_u16((172 * context->seed[1]) % WICHMANN_HILL_CONSTS[1]);
    context->seed[2] = korl_checkCast_i$_to_u16((170 * context->seed[2]) % WICHMANN_HILL_CONSTS[2]);
    return korl_math_fmod(   context->seed[0] / KORL_C_CAST(f32,WICHMANN_HILL_CONSTS[0])
                           + context->seed[1] / KORL_C_CAST(f32,WICHMANN_HILL_CONSTS[1])
                           + context->seed[2] / KORL_C_CAST(f32,WICHMANN_HILL_CONSTS[2]), 
                          1);
}
korl_internal inline f32 korl_math_rng_wichmannHill_f32_range(Korl_Math_Rng_WichmannHill*const context, f32 range0, f32 range1)
{
    const f32 rangeMax = KORL_MATH_MAX(range0, range1);
    const f32 rangeMin = KORL_MATH_MIN(range0, range1);
    return rangeMin + (rangeMax - rangeMin)*korl_math_rng_wichmannHill_f32_0_1(context);
}
korl_internal Korl_Math_V2f32 korl_math_v2f32_fromRotationZ(f32 radius, f32 radians)
{
    return KORL_STRUCT_INITIALIZE(Korl_Math_V2f32)
        {{.x = radius * cosf(radians)
         ,.y = radius * sinf(radians)}};
}
korl_internal f32 korl_math_v2f32_magnitude(const Korl_Math_V2f32*const v)
{
    return sqrtf(korl_math_v2f32_magnitudeSquared(v));
}
korl_internal f32 korl_math_v2f32_magnitudeSquared(const Korl_Math_V2f32*const v)
{
    return powf(v->elements[0], 2) + powf(v->elements[1], 2);
}
korl_internal Korl_Math_V2f32 korl_math_v2f32_normal(Korl_Math_V2f32 v)
{
    const f32 magnitude = korl_math_v2f32_magnitude(&v);
    return korl_math_v2f32_normalKnownMagnitude(v, magnitude);
}
korl_internal Korl_Math_V2f32 korl_math_v2f32_normalKnownMagnitude(Korl_Math_V2f32 v, f32 magnitude)
{
    if(korl_math_isNearlyZero(magnitude))
    {
        v.elements[0] = 0;
        v.elements[1] = 0;
        return v;
    }
    v.elements[0] /= magnitude;
    v.elements[1] /= magnitude;
    return v;
}
korl_internal f32 korl_math_v2f32_cross(Korl_Math_V2f32 vA, Korl_Math_V2f32 vB)
{
    return vA.x*vB.y - vA.y*vB.x;
}
korl_internal f32 korl_math_v2f32_dot(Korl_Math_V2f32 vA, Korl_Math_V2f32 vB)
{
    return vA.elements[0] * vB.elements[0]
        +  vA.elements[1] * vB.elements[1];
}
korl_internal f32 korl_math_v2f32_radiansBetween(Korl_Math_V2f32 vA, Korl_Math_V2f32 vB)
{
    return korl_math_acos(korl_math_v2f32_dot(korl_math_v2f32_normal(vA), korl_math_v2f32_normal(vB)));
}
korl_internal Korl_Math_V2f32 korl_math_v2f32_add(Korl_Math_V2f32 vA, Korl_Math_V2f32 vB)
{
    vA.elements[0] += vB.elements[0];
    vA.elements[1] += vB.elements[1];
    return vA;
}
korl_internal Korl_Math_V2f32 korl_math_v2f32_subtract(Korl_Math_V2f32 vA, Korl_Math_V2f32 vB)
{
    vA.elements[0] -= vB.elements[0];
    vA.elements[1] -= vB.elements[1];
    return vA;
}
korl_internal Korl_Math_V2f32 korl_math_v2f32_multiply(Korl_Math_V2f32 vA, Korl_Math_V2f32 vB)
{
    vA.elements[0] *= vB.elements[0];
    vA.elements[1] *= vB.elements[1];
    return vA;
}
korl_internal Korl_Math_V2f32 korl_math_v2f32_multiplyScalar(Korl_Math_V2f32 v, f32 scalar)
{
    v.elements[0] *= scalar;
    v.elements[1] *= scalar;
    return v;
}
korl_internal Korl_Math_V2f32 korl_math_v2f32_divide(Korl_Math_V2f32 vA, Korl_Math_V2f32 vB)
{
    vA.elements[0] /= vB.elements[0];
    vA.elements[1] /= vB.elements[1];
    return vA;
}
korl_internal void korl_math_v2f32_assignAdd(Korl_Math_V2f32*const vA, Korl_Math_V2f32 vB)
{
    vA->elements[0] += vB.elements[0];
    vA->elements[1] += vB.elements[1];
}
korl_internal void korl_math_v2f32_assignAddScalar(Korl_Math_V2f32*const v, f32 scalar)
{
    v->elements[0] += scalar;
    v->elements[1] += scalar;
}
korl_internal void korl_math_v2f32_assignSubtract(Korl_Math_V2f32*const vA, Korl_Math_V2f32 vB)
{
    vA->elements[0] -= vB.elements[0];
    vA->elements[1] -= vB.elements[1];
}
korl_internal void korl_math_v2f32_assignSubtractScalar(Korl_Math_V2f32*const v, f32 scalar)
{
    v->elements[0] -= scalar;
    v->elements[1] -= scalar;
}
korl_internal void korl_math_v2f32_assignMultiply(Korl_Math_V2f32*const vA, Korl_Math_V2f32 vB)
{
    vA->elements[0] *= vB.elements[0];
    vA->elements[1] *= vB.elements[1];
}
korl_internal Korl_Math_V2f32 korl_math_v2f32_min(Korl_Math_V2f32 vA, Korl_Math_V2f32 vB)
{
    vA.elements[0] = KORL_MATH_MIN(vA.elements[0], vB.elements[0]);
    vA.elements[1] = KORL_MATH_MIN(vA.elements[1], vB.elements[1]);
    return vA;
}
korl_internal Korl_Math_V2f32 korl_math_v2f32_max(Korl_Math_V2f32 vA, Korl_Math_V2f32 vB)
{
    vA.elements[0] = KORL_MATH_MAX(vA.elements[0], vB.elements[0]);
    vA.elements[1] = KORL_MATH_MAX(vA.elements[1], vB.elements[1]);
    return vA;
}
korl_internal f32 korl_math_v3f32_magnitude(const Korl_Math_V3f32*const v)
{
    return sqrtf(korl_math_v3f32_magnitudeSquared(v));
}
korl_internal f32 korl_math_v3f32_magnitudeSquared(const Korl_Math_V3f32*const v)
{
    return powf(v->elements[0], 2) + powf(v->elements[1], 2) + powf(v->elements[2], 2);
}
korl_internal Korl_Math_V3f32 korl_math_v3f32_normal(Korl_Math_V3f32 v)
{
    const f32 magnitude = korl_math_v3f32_magnitude(&v);
    return korl_math_v3f32_normalKnownMagnitude(v, magnitude);
}
korl_internal Korl_Math_V3f32 korl_math_v3f32_normalKnownMagnitude(Korl_Math_V3f32 v, f32 magnitude)
{
    if(korl_math_isNearlyZero(magnitude))
    {
        v.elements[0] = 0;
        v.elements[1] = 0;
        v.elements[2] = 0;
        return v;
    }
    v.elements[0] /= magnitude;
    v.elements[1] /= magnitude;
    v.elements[2] /= magnitude;
    return v;
}
korl_internal Korl_Math_V3f32 korl_math_v3f32_cross(const Korl_Math_V3f32*const vA, const Korl_Math_V3f32*const vB)
{
    Korl_Math_V3f32 result;
    result.x = vA->y*vB->z - vA->z*vB->y;
    result.y = vA->z*vB->x - vA->x*vB->z;
    result.z = vA->x*vB->y - vA->y*vB->x;
    return result;
}
korl_internal f32 korl_math_v3f32_dot(const Korl_Math_V3f32*const vA, const Korl_Math_V3f32*const vB)
{
    return vA->elements[0] * vB->elements[0]
        +  vA->elements[1] * vB->elements[1]
        +  vA->elements[2] * vB->elements[2];
}
korl_internal Korl_Math_V3f32 korl_math_v3f32_add(Korl_Math_V3f32 vA, Korl_Math_V3f32 vB)
{
    vA.elements[0] += vB.elements[0];
    vA.elements[1] += vB.elements[1];
    vA.elements[2] += vB.elements[2];
    return vA;
}
korl_internal Korl_Math_V3f32 korl_math_v3f32_subtract(Korl_Math_V3f32 vA, Korl_Math_V3f32 vB)
{
    vA.elements[0] -= vB.elements[0];
    vA.elements[1] -= vB.elements[1];
    vA.elements[2] -= vB.elements[2];
    return vA;
}
korl_internal Korl_Math_V3f32 korl_math_v3f32_multiplyScalar(Korl_Math_V3f32 v, f32 scalar)
{
    v.elements[0] *= scalar;
    v.elements[1] *= scalar;
    v.elements[2] *= scalar;
    return v;
}
korl_internal f32 korl_math_v4f32_magnitude(const Korl_Math_V4f32*const v)
{
    return sqrtf(korl_math_v4f32_magnitudeSquared(v));
}
korl_internal f32 korl_math_v4f32_magnitudeSquared(const Korl_Math_V4f32*const v)
{
    return powf(v->elements[0], 2) + powf(v->elements[1], 2) + powf(v->elements[2], 2) + powf(v->elements[3], 2);
}
korl_internal Korl_Math_V4f32 korl_math_v4f32_normal(Korl_Math_V4f32 v)
{
    const f32 magnitude = korl_math_v4f32_magnitude(&v);
    return korl_math_v4f32_normalKnownMagnitude(v, magnitude);
}
korl_internal Korl_Math_V4f32 korl_math_v4f32_normalKnownMagnitude(Korl_Math_V4f32 v, f32 magnitude)
{
    if(korl_math_isNearlyZero(magnitude))
    {
        v.elements[0] = 0;
        v.elements[1] = 0;
        v.elements[2] = 0;
        v.elements[3] = 0;
        return v;
    }
    v.elements[0] /= magnitude;
    v.elements[1] /= magnitude;
    v.elements[2] /= magnitude;
    v.elements[3] /= magnitude;
    return v;
}
korl_internal f32 korl_math_v4f32_dot(const Korl_Math_V4f32*const vA, const Korl_Math_V4f32*const vB)
{
    return vA->elements[0] * vB->elements[0]
        +  vA->elements[1] * vB->elements[1]
        +  vA->elements[2] * vB->elements[2]
        +  vA->elements[3] * vB->elements[3];
}
korl_internal Korl_Math_Quaternion korl_math_quaternion_fromAxisRadians(Korl_Math_V3f32 axis, f32 radians, bool axisIsNormalized)
{
    if(!axisIsNormalized)
        axis = korl_math_v3f32_normal(axis);
    const f32 sine = sinf(radians/2);
    return KORL_STRUCT_INITIALIZE(Korl_Math_Quaternion){
        sine * axis.x, 
        sine * axis.y, 
        sine * axis.z, 
        cosf(radians/2) };
}
korl_internal Korl_Math_Quaternion korl_math_quaternion_multiply(Korl_Math_Quaternion qA, Korl_Math_Quaternion qB)
{
    return korl_math_quaternion_hamilton(qA, qB);
}
korl_internal Korl_Math_Quaternion korl_math_quaternion_hamilton(Korl_Math_Quaternion qA, Korl_Math_Quaternion qB)
{
    return KORL_STRUCT_INITIALIZE(Korl_Math_Quaternion){
        qA.w*qB.x + qA.x*qB.w + qA.y*qB.z - qA.z*qB.y,
        qA.w*qB.y - qA.x*qB.z + qA.y*qB.w + qA.z*qB.x,
        qA.w*qB.z + qA.x*qB.y - qA.y*qB.x + qA.z*qB.w,
        qA.w*qB.w - qA.x*qB.x - qA.y*qB.y - qA.z*qB.z };
}
korl_internal Korl_Math_Quaternion korl_math_quaternion_conjugate(Korl_Math_Quaternion q)
{
	return KORL_STRUCT_INITIALIZE(Korl_Math_Quaternion){-q.x, -q.y, -q.z, q.w};
}
korl_internal Korl_Math_V2f32 korl_math_quaternion_transformV2f32(Korl_Math_Quaternion q, Korl_Math_V2f32 v, bool qIsNormalized)
{
    if(!qIsNormalized)
        q = korl_math_v4f32_normal(q);
    q = korl_math_quaternion_hamilton(
            korl_math_quaternion_hamilton(q, KORL_STRUCT_INITIALIZE(Korl_Math_Quaternion){v.x, v.y, 0.f, 0.f}), 
            korl_math_quaternion_conjugate(q));
    return q.xy;
}
korl_internal Korl_Math_V3f32 korl_math_quaternion_transformV3f32(Korl_Math_Quaternion q, Korl_Math_V3f32 v, bool qIsNormalized)
{
    if(!qIsNormalized)
        q = korl_math_v4f32_normal(q);
    q = korl_math_quaternion_hamilton(
        korl_math_quaternion_hamilton(q, KORL_STRUCT_INITIALIZE(Korl_Math_Quaternion){v.x, v.y, v.z, 0.f}), 
        korl_math_quaternion_conjugate(q));
    return q.xyz;
}
korl_internal Korl_Math_M4f32 korl_math_makeM4f32_rotate(Korl_Math_Quaternion qRotation)
{
    KORL_MATH_ZERO_STACK(Korl_Math_M4f32, result);
    /* https://www.euclideanspace.com/maths/geometry/rotations/conversions/quaternionToMatrix */
    const f32 sqW = qRotation.w*qRotation.w;
    const f32 sqX = qRotation.x*qRotation.x;
    const f32 sqY = qRotation.y*qRotation.y;
    const f32 sqZ = qRotation.z*qRotation.z;
    const f32 inverseSquareLength = 1 / (sqW + sqX + sqY + sqZ);
    result.r0c0 = ( sqX - sqY - sqZ + sqW) * inverseSquareLength;
    result.r1c1 = (-sqX + sqY - sqZ + sqW) * inverseSquareLength;
    result.r2c2 = (-sqX - sqY + sqZ + sqW) * inverseSquareLength;
    const f32 temp1 = qRotation.x*qRotation.y;
    const f32 temp2 = qRotation.z*qRotation.w;
    result.r1c0 = 2*(temp1 + temp2) * inverseSquareLength;
    result.r0c1 = 2*(temp1 - temp2) * inverseSquareLength;
    const f32 temp3 = qRotation.x*qRotation.z;
    const f32 temp4 = qRotation.y*qRotation.w;
    result.r2c0 = 2*(temp3 - temp4) * inverseSquareLength;
    result.r0c2 = 2*(temp3 + temp4) * inverseSquareLength;
    const f32 temp5 = qRotation.y*qRotation.z;
    const f32 temp6 = qRotation.x*qRotation.w;
    result.r2c1 = 2*(temp5 + temp6) * inverseSquareLength;
    result.r1c2 = 2*(temp5 - temp6) * inverseSquareLength;
    result.r0c3 = result.r1c3 = result.r2c3 = 0;
    result.r3c0 = result.r3c1 = result.r3c2 = 0;
    result.r3c3 = 1;
    return result;
}
korl_internal Korl_Math_M4f32 korl_math_makeM4f32_rotateTranslate(Korl_Math_Quaternion qRotation, Korl_Math_V3f32 vTranslation)
{
    //KORL-PERFORMANCE-000-000-004
    Korl_Math_M4f32 m4Rotation = korl_math_makeM4f32_rotate(qRotation);
    Korl_Math_M4f32 m4Translation = KORL_MATH_M4F32_IDENTITY;
    m4Translation.r0c3 = vTranslation.x;
    m4Translation.r1c3 = vTranslation.y;
    m4Translation.r2c3 = vTranslation.z;
    return korl_math_m4f32_multiply(&m4Translation, &m4Rotation);
}
korl_internal Korl_Math_M4f32 korl_math_makeM4f32_rotateScaleTranslate(Korl_Math_Quaternion qRotation, Korl_Math_V3f32 vScale, Korl_Math_V3f32 vTranslation)
{
    //KORL-PERFORMANCE-000-000-004
    Korl_Math_M4f32 m4Rotation = korl_math_makeM4f32_rotate(qRotation);
    Korl_Math_M4f32 m4Translation = KORL_MATH_M4F32_IDENTITY;
    m4Translation.r0c3 = vTranslation.x;
    m4Translation.r1c3 = vTranslation.y;
    m4Translation.r2c3 = vTranslation.z;
    Korl_Math_M4f32 m4Scale = KORL_MATH_M4F32_IDENTITY;
	m4Scale.r0c0 = vScale.x;
	m4Scale.r1c1 = vScale.y;
	m4Scale.r2c2 = vScale.z;
    Korl_Math_M4f32 m4ScaleRotate = korl_math_m4f32_multiply(&m4Rotation, &m4Scale);
    return korl_math_m4f32_multiply(&m4Translation, &m4ScaleRotate);
}
korl_internal Korl_Math_M4f32 korl_math_m4f32_transpose(const Korl_Math_M4f32*const m)
{
    KORL_MATH_ZERO_STACK(Korl_Math_M4f32, result);
    for(unsigned r = 0; r < 4; r++)
        for(unsigned c = 0; c < 4; c++)
            result.rows[r].elements[c] = m->rows[c].elements[r];
    return result;
}
#pragma warning(push)
#pragma warning(disable:4701)/* uninitialized local variable - trust me bro I know what I'm doing here */
korl_internal Korl_Math_M4f32 korl_math_m4f32_multiply(const Korl_Math_M4f32*const mA, const Korl_Math_M4f32*const mB)
{
    //KORL-PERFORMANCE-000-000-005
    /** used to perform dot product with the columns of mB */
    const Korl_Math_M4f32 mBTranspose = korl_math_m4f32_transpose(mB);
    Korl_Math_M4f32 result;// no need to initialize to anything, since we will write to all elements
    for(u8 mARow = 0; mARow < 4; mARow++)
        for(u8 mBCol = 0; mBCol < 4; mBCol++)
            result.elements[mARow*4 + mBCol] = korl_math_v4f32_dot(&mA->rows[mARow], &mBTranspose.rows[mBCol]);
    return result;
}
#pragma warning(pop)
korl_internal Korl_Math_M4f32 korl_math_m4f32_projectionFov(
    f32 horizontalFovDegrees, f32 viewportWidthOverHeight, 
    f32 clipNear, f32 clipFar)
{
    KORL_MATH_ZERO_STACK(Korl_Math_M4f32, result);
    KORL_MATH_ASSERT(!korl_math_isNearlyZero(horizontalFovDegrees));
    KORL_MATH_ASSERT(!korl_math_isNearlyEqual(clipNear, clipFar) && clipNear < clipFar);
    const f32 horizontalFovRadians = horizontalFovDegrees*KORL_PI32/180.f;
    const f32 tanHalfFov = tanf(horizontalFovRadians / 2.f);
    /* Good sources for how to derive this matrix:
        http://ogldev.org/www/tutorial12/tutorial12.html
        http://www.songho.ca/opengl/gl_projectionmatrix.html */
    /* transforms eye-space into clip-space with the following properties:
        - left-handed, because Vulkan default requires VkViewport.min/maxDepth 
          values to lie within the range [0,1] (unless the extension 
          `VK_EXT_depth_range_unrestricted` is used, but who wants to do that?)
        - clipNear maps to Z==0 in clip-space
        - clipFar  maps to Z==1 in clip-space */
    result.r0c0 = 1.f / tanHalfFov;
    result.r1c1 = viewportWidthOverHeight / tanHalfFov;
    result.r2c2 =            -clipFar  / (clipNear - clipFar);
    result.r2c3 = (clipNear * clipFar) / (clipNear - clipFar);
    result.r3c2 = 1;
    return result;
}
korl_internal Korl_Math_M4f32 korl_math_m4f32_projectionOrthographic(
    f32 xMin, f32 xMax, f32 yMin, f32 yMax, f32 zMin, f32 zMax)
{
    KORL_MATH_ASSERT(!korl_math_isNearlyZero(xMin - xMax));
    KORL_MATH_ASSERT(!korl_math_isNearlyZero(yMin - yMax));
    KORL_MATH_ASSERT(!korl_math_isNearlyZero(xMin - xMax));
    /* derive orthographic matrix using the power of math, people! 
        - we're given 6 coordinates which define the AABB in eye-space that we 
          want to map into clip-space 
        - we're assuming here that eye-space is right-handed
        - clip-space coordinates shall be left-handed, because Vulkan depth 
          values must be in the range [0,1] without extensions
            - zMin maps to 0
            - zMax maps to 1 */
    KORL_MATH_ZERO_STACK(Korl_Math_M4f32, result);
    result.r0c0 =              2 / (xMax - xMin);
    result.r0c3 = -(xMax + xMin) / (xMax - xMin);
    result.r1c1 =              2 / (yMax - yMin);
    result.r1c3 =  (yMax + yMin) / (yMax - yMin);
    result.r2c2 =    1 / (zMin - zMax);
    result.r2c3 = zMin / (zMin - zMax);
    result.r3c3 = 1;
    return result;
}
korl_internal Korl_Math_M4f32 korl_math_m4f32_lookAt(
    const Korl_Math_V3f32*const positionEye, 
    const Korl_Math_V3f32*const positionTarget, 
    const Korl_Math_V3f32*const worldUpNormal)
{
    //KORL-PERFORMANCE-000-000-006
    /* sanity check to ensure that worldUpNormal is, in fact, normalized! */
    KORL_MATH_ASSERT(korl_math_isNearlyEqual(korl_math_v3f32_magnitudeSquared(worldUpNormal), 1));
    Korl_Math_M4f32 result = KORL_MATH_M4F32_IDENTITY;
    /* Even though this author uses "column-major" memory layout for matrices 
        for some reason, which makes everything really fucking confusing most of 
        the time, this still does a good job explaining a useful implementation 
        of the view matrix: https://www.3dgep.com/understanding-the-view-matrix/ */
    /* our camera eye-space will be left-handed, with the camera looking down 
        the +Z axis, and +Y being UP */
    const Korl_Math_V3f32 cameraZ = korl_math_v3f32_normal(korl_math_v3f32_subtract(*positionTarget, *positionEye));
    const Korl_Math_V3f32 cameraX = korl_math_v3f32_cross(&cameraZ, worldUpNormal);
    const Korl_Math_V3f32 cameraY = korl_math_v3f32_cross(&cameraZ, &cameraX);
    /* the view matrix is simply the inverse of the camera transform, and since 
        the camera transform is orthonormal, we can simply transpose the 
        elements of the rotation matrix */
    /* additionally, we can combine the inverse camera translation into the same 
        matrix as the inverse rotation by manually doing the row-column dot 
        products ourselves, and putting them in the 4th column */
    result.r0c0 = cameraX.x; result.r0c1 = cameraX.y; result.r0c2 = cameraX.z; result.r0c3 = -korl_math_v3f32_dot(&cameraX, positionEye);
    result.r1c0 = cameraY.x; result.r1c1 = cameraY.y; result.r1c2 = cameraY.z; result.r1c3 = -korl_math_v3f32_dot(&cameraY, positionEye);
    result.r2c0 = cameraZ.x; result.r2c1 = cameraZ.y; result.r2c2 = cameraZ.z; result.r2c3 = -korl_math_v3f32_dot(&cameraZ, positionEye);
    return result;
}
korl_internal bool korl_math_m4f32_isNearEqual(const Korl_Math_M4f32*const mA, const Korl_Math_M4f32*const mB)
{
    for(u8 i = 0; i < korl_arraySize(mA->elements); i++)
        if(!korl_math_isNearlyEqual(mA->elements[i], mB->elements[i]))
            return false;
    return true;
}
korl_internal Korl_Math_Aabb2f32 korl_math_aabb2f32_fromPoints(f32 p0x, f32 p0y, f32 p1x, f32 p1y)
{
    Korl_Math_Aabb2f32 result;
    result.min.x = KORL_MATH_MIN(p0x, p1x);
    result.min.y = KORL_MATH_MIN(p0y, p1y);
    result.max.x = KORL_MATH_MAX(p0x, p1x);
    result.max.y = KORL_MATH_MAX(p0y, p1y);
    return result;
}
korl_internal Korl_Math_Aabb2f32 korl_math_aabb2f32_fromExpanded(Korl_Math_Aabb2f32 aabb, f32 expandX, f32 expandY)
{
    const Korl_Math_V2f32 expandV2 = {expandX, expandY};
    return KORL_STRUCT_INITIALIZE(Korl_Math_Aabb2f32){.min = korl_math_v2f32_subtract(aabb.min, expandV2), 
                                                      .max = korl_math_v2f32_add     (aabb.max, expandV2)};
}
korl_internal Korl_Math_Aabb2f32 korl_math_aabb2f32_fromExpandedV2(Korl_Math_V2f32 v, f32 expandX, f32 expandY)
{
    const Korl_Math_V2f32 expandV2 = {expandX, expandY};
    return KORL_STRUCT_INITIALIZE(Korl_Math_Aabb2f32){.min = korl_math_v2f32_subtract(v, expandV2), 
                                                      .max = korl_math_v2f32_add     (v, expandV2)};
}
korl_internal void korl_math_aabb2f32_expand(Korl_Math_Aabb2f32*const aabb, f32 expandXY)
{
    korl_math_v2f32_assignSubtractScalar(&aabb->min, expandXY);
    korl_math_v2f32_assignAddScalar     (&aabb->max, expandXY);
}
korl_internal Korl_Math_V2f32 korl_math_aabb2f32_size(Korl_Math_Aabb2f32 aabb)
{
    KORL_MATH_ASSERT(aabb.max.x >= aabb.min.x);
    KORL_MATH_ASSERT(aabb.max.y >= aabb.min.y);
    return korl_math_v2f32_subtract(aabb.max, aabb.min);
}
korl_internal bool korl_math_aabb2f32_contains(Korl_Math_Aabb2f32 aabb, f32 x, f32 y)
{
    return x >= aabb.min.x && x <= aabb.max.x 
        && y >= aabb.min.y && y <= aabb.max.y;
}
korl_internal bool korl_math_aabb2f32_containsV2f32(Korl_Math_Aabb2f32 aabb, Korl_Math_V2f32 v)
{
    return korl_math_aabb2f32_contains(aabb, v.x, v.y);
}
korl_internal Korl_Math_Aabb2f32 korl_math_aabb2f32_union(Korl_Math_Aabb2f32 aabbA, Korl_Math_Aabb2f32 aabbB)
{
    Korl_Math_Aabb2f32 result;
    result.min.x = KORL_MATH_MIN(aabbA.min.x, aabbB.min.x);
    result.min.y = KORL_MATH_MIN(aabbA.min.y, aabbB.min.y);
    result.max.x = KORL_MATH_MAX(aabbA.max.x, aabbB.max.x);
    result.max.y = KORL_MATH_MAX(aabbA.max.y, aabbB.max.y);
    return result;
}
korl_internal Korl_Math_Aabb3f32 korl_math_aabb3f32_fromPoints(f32 p0x, f32 p0y, f32 p0z, f32 p1x, f32 p1y, f32 p1z)
{
    Korl_Math_Aabb3f32 result;
    result.min.x = KORL_MATH_MIN(p0x, p1x);
    result.min.y = KORL_MATH_MIN(p0y, p1y);
    result.min.z = KORL_MATH_MIN(p0z, p1z);
    result.max.x = KORL_MATH_MAX(p0x, p1x);
    result.max.y = KORL_MATH_MAX(p0y, p1y);
    result.max.z = KORL_MATH_MAX(p0z, p1z);
    return result;
}
korl_internal Korl_Math_V3f32 korl_math_aabb3f32_size(Korl_Math_Aabb3f32 aabb)
{
    KORL_MATH_ASSERT(aabb.max.x >= aabb.min.x);
    KORL_MATH_ASSERT(aabb.max.y >= aabb.min.y);
    KORL_MATH_ASSERT(aabb.max.z >= aabb.min.z);
    return korl_math_v3f32_subtract(aabb.max, aabb.min);
}
korl_internal bool korl_math_aabb3f32_contains(Korl_Math_Aabb3f32 aabb, f32 x, f32 y, f32 z)
{
    return x >= aabb.min.x && x <= aabb.max.x 
        && y >= aabb.min.y && y <= aabb.max.y 
        && z >= aabb.min.z && z <= aabb.max.z;
}
korl_internal bool korl_math_aabb3f32_containsV3f32(Korl_Math_Aabb3f32 aabb, Korl_Math_V3f32 v)
{
    return korl_math_aabb3f32_contains(aabb, v.x, v.y, v.z);
}
korl_internal Korl_Math_Aabb3f32 korl_math_aabb3f32_union(Korl_Math_Aabb3f32 aabbA, Korl_Math_Aabb3f32 aabbB)
{
    Korl_Math_Aabb3f32 result;
    result.min.x = KORL_MATH_MIN(aabbA.min.x, aabbB.min.x);
    result.min.y = KORL_MATH_MIN(aabbA.min.y, aabbB.min.y);
    result.min.z = KORL_MATH_MIN(aabbA.min.z, aabbB.min.z);
    result.max.x = KORL_MATH_MAX(aabbA.max.x, aabbB.max.x);
    result.max.y = KORL_MATH_MAX(aabbA.max.y, aabbB.max.y);
    result.max.z = KORL_MATH_MAX(aabbA.max.z, aabbB.max.z);
    return result;
}
#ifdef __cplusplus
korl_internal Korl_Math_Quaternion operator*(Korl_Math_Quaternion qA, Korl_Math_Quaternion qB)
{
    return korl_math_quaternion_multiply(qA, qB);
}
korl_internal Korl_Math_V2f32 operator*(Korl_Math_Quaternion q, Korl_Math_V2f32 v)
{
    return korl_math_quaternion_transformV2f32(q, v, false);
}
korl_internal Korl_Math_V2f32 operator+(Korl_Math_V2f32 vA, Korl_Math_V2f32 vB)
{
    return korl_math_v2f32_add(vA, vB);
}
korl_internal Korl_Math_V2f32 operator-(Korl_Math_V2f32 v)
{
	return korl_math_v2f32_multiplyScalar(v, -1.f);
}
korl_internal Korl_Math_V2f32 operator-(Korl_Math_V2f32 vA, Korl_Math_V2f32 vB)
{
    return korl_math_v2f32_subtract(vA, vB);
}
korl_internal Korl_Math_V2f32 operator*(Korl_Math_V2f32 v, f32 scalar)
{
    return korl_math_v2f32_multiplyScalar(v, scalar);
}
korl_internal Korl_Math_V2f32 operator*(f32 scalar, Korl_Math_V2f32 v)
{
    return korl_math_v2f32_multiplyScalar(v, scalar);
}
korl_internal Korl_Math_V2f32& operator+=(Korl_Math_V2f32& vA, Korl_Math_V2f32 vB)
{
	vA.x += vB.x;
	vA.y += vB.y;
	return vA;
}
korl_internal Korl_Math_V2f32& operator-=(Korl_Math_V2f32& vA, Korl_Math_V2f32 vB)
{
	vA.x -= vB.x;
	vA.y -= vB.y;
	return vA;
}
#endif//__cplusplus
