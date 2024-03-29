#include "utility/korl-utility-math.h"
#include "utility/korl-checkCast.h"
#include "utility/korl-utility-stb-ds.h"
#include "korl-interface-platform.h"
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
    const f32 diff = korl_math_f32_positive(fA - fB);
    return diff <= epsilon;
    //KORL-ISSUE-000-000-085: math: isNearlyEqualEpsilon code doesn't work and I don't know why
    /**  Thanks, Bruce Dawson!  Source: 
     * https://randomascii.wordpress.com/2012/02/25/comparing-floating-point-numbers-2012-edition/
     */
    // fA = korl_math_f32_positive(fA);
    // fB = korl_math_f32_positive(fB);
    // const f32 largest = (fB > fA) ? fB : fA;
    // return (diff <= largest * epsilon);
}
korl_internal inline bool korl_math_isNearlyEqual(f32 fA, f32 fB)
{
    return korl_math_isNearlyEqualEpsilon(fA, fB, 1e-5f);
}
korl_internal inline f32 korl_math_f32_positive(f32 x)
{
    if(x >= 0)
        return x;
    return -x;
}
korl_internal inline f32 korl_math_f32_lerp(f32 from, f32 to, f32 factor)
{
    return from + factor * (to - from);
}
korl_internal inline f32 korl_math_f32_exponentialDecay(f32 from, f32 to, f32 lambdaFactor, f32 deltaTime)
{
    // good resource on this:  
    //  https://www.rorydriscoll.com/2016/03/07/frame-rate-independent-damping-using-lerp/
    return korl_math_f32_lerp(to, from, korl_math_f32_exponential(-lambdaFactor * deltaTime));
}
korl_internal u8 korl_math_solveQuadraticReal(f32 a, f32 b, f32 c, f32 o_roots[2])
{
    /* https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-sphere-intersection */
    const f32 discriminant = b*b - 4*a*c;
    if(discriminant < 0)/* no real# solution */
        return 0;
    if(korl_math_isNearlyZero(a))/* degenerate line equation case */
    {
        if(korl_math_isNearlyZero(b))/* horizontal line case: either NO SOLUTION or INFINITE */
        {
            if(korl_math_isNearlyZero(c))
                o_roots[0] = korl_math_f32_infinity();// super-rare case
            else
                o_roots[0] = 0;
            return 0;
        }
        o_roots[0] = -c / b;
        return 1;
    }
    u8 result = 1;
    if(korl_math_isNearlyZero(discriminant))/* rare case: only ONE real# solution */
        o_roots[0] = -0.5f * b / a;
    else
    {
        result = 2;
        const f32 q = b > 0
                      ? -0.5f * (b + korl_math_f32_squareRoot(discriminant))
                      : -0.5f * (b - korl_math_f32_squareRoot(discriminant));
        /* given the cases handled above, it should not be possible for q to 
            approach zero, but just in case let's check that here... same for a */
        KORL_MATH_ASSERT(!korl_math_isNearlyZero(a));
        KORL_MATH_ASSERT(!korl_math_isNearlyZero(q));
        o_roots[0] = q/a;
        o_roots[1] = c/q;
        if(o_roots[0] > o_roots[1])// ensure that the result roots are in ascending order
        {
            const f32 temp = o_roots[0];
            o_roots[0] = o_roots[1];
            o_roots[1] = temp;
        }
    }
    return result;
}
korl_internal u$ korl_math_generateMeshSphereVertexCount(u32 latitudeSegments, u32 longitudeSegments)
{
    return ((latitudeSegments - 2)*6 + 6) * longitudeSegments;
}
korl_internal void korl_math_generateMeshSphere(f32 radius, u32 latitudeSegments, u32 longitudeSegments, Korl_Math_V3f32* o_vertexPositions, u$ vertexPositionByteStride, Korl_Math_V2f32* o_vertexTextureNormals, u$ vertexTextureNormalByteStride)
{
    korl_assert(latitudeSegments  >= 2);
    korl_assert(longitudeSegments >= 3);
    const u$ requiredVertexCount = korl_math_generateMeshSphereVertexCount(latitudeSegments, longitudeSegments);
    const f32             radiansPerSemiLongitude = 2*KORL_PI32 / KORL_C_CAST(f32, longitudeSegments);
    const f32             radiansPerLatitude      =   KORL_PI32 / KORL_C_CAST(f32, latitudeSegments);
    const Korl_Math_V3f32 verticalRadius          = korl_math_v3f32_make(0, 0, radius);
    u$ currentVertex = 0;
    for(u32 longitude = 1; longitude <= longitudeSegments; longitude++)
    {
        /* add the next triangle to the top cap */
        const Korl_Math_Quaternion quatLongitude           = korl_math_quaternion_fromAxisRadians(KORL_MATH_V3F32_Z, KORL_C_CAST(f32, longitude)     * radiansPerSemiLongitude, true);
        const Korl_Math_Quaternion quatLongitudePrevious   = korl_math_quaternion_fromAxisRadians(KORL_MATH_V3F32_Z, KORL_C_CAST(f32, longitude - 1) * radiansPerSemiLongitude, true);
        const Korl_Math_V3f32      capTopZeroLongitude     = korl_math_quaternion_transformV3f32(korl_math_quaternion_fromAxisRadians(KORL_MATH_V3F32_Y, radiansPerLatitude, true), verticalRadius, true);
        const Korl_Math_V3f32      capTopLongitudeCurrent  = korl_math_quaternion_transformV3f32(quatLongitude        , capTopZeroLongitude, true);
        const Korl_Math_V3f32      capTopLongitudePrevious = korl_math_quaternion_transformV3f32(quatLongitudePrevious, capTopZeroLongitude, true);
        *KORL_C_CAST(Korl_Math_V3f32*, KORL_C_CAST(u8*, o_vertexPositions) + vertexPositionByteStride * currentVertex++) = capTopLongitudePrevious;
        *KORL_C_CAST(Korl_Math_V3f32*, KORL_C_CAST(u8*, o_vertexPositions) + vertexPositionByteStride * currentVertex++) = capTopLongitudeCurrent;
        *KORL_C_CAST(Korl_Math_V3f32*, KORL_C_CAST(u8*, o_vertexPositions) + vertexPositionByteStride * currentVertex++) = verticalRadius;
        /* add the next triangle to the bottom cap */
        *KORL_C_CAST(Korl_Math_V3f32*, KORL_C_CAST(u8*, o_vertexPositions) + vertexPositionByteStride * currentVertex) = capTopLongitudeCurrent;
        KORL_C_CAST(Korl_Math_V3f32*, KORL_C_CAST(u8*, o_vertexPositions) + vertexPositionByteStride * currentVertex++)->z *= -1;
        *KORL_C_CAST(Korl_Math_V3f32*, KORL_C_CAST(u8*, o_vertexPositions) + vertexPositionByteStride * currentVertex) = capTopLongitudePrevious;
        KORL_C_CAST(Korl_Math_V3f32*, KORL_C_CAST(u8*, o_vertexPositions) + vertexPositionByteStride * currentVertex++)->z *= -1;
        *KORL_C_CAST(Korl_Math_V3f32*, KORL_C_CAST(u8*, o_vertexPositions) + vertexPositionByteStride * currentVertex++) = korl_math_v3f32_multiplyScalar(verticalRadius, -1);
        /* add the quads to the internal latitude strips */
        for(u32 latitude = 1; latitude < latitudeSegments - 1; latitude++)
        {
            const Korl_Math_Quaternion quatLatitudePrevious = korl_math_quaternion_fromAxisRadians(KORL_MATH_V3F32_Y, KORL_C_CAST(f32, latitude)     * radiansPerLatitude, true);
            const Korl_Math_Quaternion quatLatitude         = korl_math_quaternion_fromAxisRadians(KORL_MATH_V3F32_Y, KORL_C_CAST(f32, latitude + 1) * radiansPerLatitude, true);
            const Korl_Math_V3f32      latStripTopPrevious     = korl_math_quaternion_transformV3f32(korl_math_quaternion_hamilton(quatLongitudePrevious, quatLatitudePrevious), verticalRadius, true);
            const Korl_Math_V3f32      latStripBottomPrevious  = korl_math_quaternion_transformV3f32(korl_math_quaternion_hamilton(quatLongitudePrevious, quatLatitude)        , verticalRadius, true);
            const Korl_Math_V3f32      latStripTop             = korl_math_quaternion_transformV3f32(korl_math_quaternion_hamilton(quatLongitude        , quatLatitudePrevious), verticalRadius, true);
            const Korl_Math_V3f32      latStripBottom          = korl_math_quaternion_transformV3f32(korl_math_quaternion_hamilton(quatLongitude        , quatLatitude)        , verticalRadius, true);
            *KORL_C_CAST(Korl_Math_V3f32*, KORL_C_CAST(u8*, o_vertexPositions) + vertexPositionByteStride * currentVertex++) = latStripTopPrevious;
            *KORL_C_CAST(Korl_Math_V3f32*, KORL_C_CAST(u8*, o_vertexPositions) + vertexPositionByteStride * currentVertex++) = latStripBottomPrevious;
            *KORL_C_CAST(Korl_Math_V3f32*, KORL_C_CAST(u8*, o_vertexPositions) + vertexPositionByteStride * currentVertex++) = latStripBottom;
            *KORL_C_CAST(Korl_Math_V3f32*, KORL_C_CAST(u8*, o_vertexPositions) + vertexPositionByteStride * currentVertex++) = latStripBottom;
            *KORL_C_CAST(Korl_Math_V3f32*, KORL_C_CAST(u8*, o_vertexPositions) + vertexPositionByteStride * currentVertex++) = latStripTop;
            *KORL_C_CAST(Korl_Math_V3f32*, KORL_C_CAST(u8*, o_vertexPositions) + vertexPositionByteStride * currentVertex++) = latStripTopPrevious;
        }
    }
    korl_assert(currentVertex == requiredVertexCount);
    if(o_vertexTextureNormals)
    {
        /* calculate proper texture normals based on cylindrical projection 
            Source: https://gamedev.stackexchange.com/a/114416 */
        //KORL-ISSUE-000-000-166: math: glitchy generated sphere mesh UV coordinates
        for(size_t v = 0; v < requiredVertexCount; v++)
        {
            const Korl_Math_V3f32 positionNorm = korl_math_v3f32_normal(*KORL_C_CAST(Korl_Math_V3f32*, KORL_C_CAST(u8*, o_vertexPositions) + vertexPositionByteStride * v));
            KORL_C_CAST(Korl_Math_V2f32*, KORL_C_CAST(u8*, o_vertexTextureNormals) + vertexTextureNormalByteStride * v)->x = 1.f - (korl_math_arcTangent(positionNorm.x, positionNorm.y) / (2 * KORL_PI32) + 0.5f);
            KORL_C_CAST(Korl_Math_V2f32*, KORL_C_CAST(u8*, o_vertexTextureNormals) + vertexTextureNormalByteStride * v)->y = 1.f - (positionNorm.z * 0.5f + 0.5f);
        }
    }
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
    return korl_math_f32_modulus(   context->seed[0] / KORL_C_CAST(f32,WICHMANN_HILL_CONSTS[0])
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
korl_internal Korl_Math_V2u32 korl_math_v2u32_addScalar(Korl_Math_V2u32 v, u32 scalar)
{
    v.elements[0] += scalar;
    v.elements[1] += scalar;
    return v;
}
korl_internal Korl_Math_V2u32 korl_math_v2u32_divide(Korl_Math_V2u32 vA, Korl_Math_V2u32 vB)
{
    vA.elements[0] /= vB.elements[0];
    vA.elements[1] /= vB.elements[1];
    return vA;
}
korl_internal Korl_Math_V2f32 korl_math_v2f32_fromV2u32(Korl_Math_V2u32 v)
{
    return korl_math_v2f32_make(korl_checkCast_u$_to_f32(v.x), korl_checkCast_u$_to_f32(v.y));
}
korl_internal Korl_Math_V2f32 korl_math_v2f32_fromRotationZ(f32 radius, f32 radians)
{
    return korl_math_v2f32_make(radius * korl_math_cosine(radians)
                               ,radius * korl_math_sine(radians));
}
korl_internal Korl_Math_V2f32 korl_math_v2f32_rotateHalfPiZ(Korl_Math_V2f32 v)
{
    return korl_math_v2f32_make(-v.y, v.x);
}
korl_internal f32 korl_math_v2f32_magnitude(const Korl_Math_V2f32*const v)
{
    return korl_math_f32_squareRoot(korl_math_v2f32_magnitudeSquared(v));
}
korl_internal f32 korl_math_v2f32_magnitudeSquared(const Korl_Math_V2f32*const v)
{
    return korl_math_f32_power(v->elements[0], 2) 
         + korl_math_f32_power(v->elements[1], 2);
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
         + vA.elements[1] * vB.elements[1];
}
korl_internal f32 korl_math_v2f32_radiansBetween(Korl_Math_V2f32 vA, Korl_Math_V2f32 vB)
{
    return korl_math_arcCosine(korl_math_v2f32_dot(korl_math_v2f32_normal(vA), korl_math_v2f32_normal(vB)));
}
korl_internal f32 korl_math_v2f32_radiansZ(Korl_Math_V2f32 v)
{
    return korl_math_arcTangent(v.y, v.x);
}
korl_internal Korl_Math_V2f32 korl_math_v2f32_project(Korl_Math_V2f32 vA, Korl_Math_V2f32 vB, bool vbIsNormalized)
{
    if(!vbIsNormalized)
        vB = korl_math_v2f32_normal(vB);
    const f32 scalarProjection = korl_math_v2f32_dot(vA, vB);
    return korl_math_v2f32_multiplyScalar(vB, scalarProjection);
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
korl_internal Korl_Math_V2f32 korl_math_v2f32_divideScalar(Korl_Math_V2f32 v, f32 scalar)
{
    v.elements[0] /= scalar;
    v.elements[1] /= scalar;
    return v;
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
korl_internal void korl_math_v2f32_assignMultiplyScalar(Korl_Math_V2f32*const v, f32 scalar)
{
    v->elements[0] *= scalar;
    v->elements[1] *= scalar;
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
korl_internal Korl_Math_V2f32 korl_math_v2f32_nan(void)
{
    return korl_math_v2f32_make(korl_math_f32_quietNan(), korl_math_f32_quietNan());
}
korl_internal bool korl_math_v2f32_hasNan(Korl_Math_V2f32 v)
{
    for(u8 i = 0; i < korl_arraySize(v.elements); i++)
        if(korl_math_f32_isNan(v.elements[i]))
            return true;
    return false;
}
korl_internal Korl_Math_V2f32 korl_math_v2f32_positive(Korl_Math_V2f32 v)
{
    return korl_math_v2f32_make(korl_math_f32_positive(v.x)
                               ,korl_math_f32_positive(v.y));
}
korl_internal bool korl_math_v2f32_isNearlyZero(Korl_Math_V2f32 v)
{
    return korl_math_isNearlyZero(v.elements[0]) 
        && korl_math_isNearlyZero(v.elements[1]);
}
korl_internal Korl_Math_V3f32 korl_math_v3f32_fromV2f32Z(Korl_Math_V2f32 v2, f32 z)
{
    return korl_math_v3f32_make(v2.x, v2.y, z);
}
korl_internal f32 korl_math_v3f32_magnitude(const Korl_Math_V3f32*const v)
{
    return korl_math_f32_squareRoot(korl_math_v3f32_magnitudeSquared(v));
}
korl_internal f32 korl_math_v3f32_magnitudeSquared(const Korl_Math_V3f32*const v)
{
    return korl_math_f32_power(v->elements[0], 2) 
         + korl_math_f32_power(v->elements[1], 2) 
         + korl_math_f32_power(v->elements[2], 2);
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
korl_internal f32 korl_math_v3f32_normalize(Korl_Math_V3f32* v)
{
    const f32 magnitude = korl_math_v3f32_magnitude(v);
    *v = korl_math_v3f32_normalKnownMagnitude(*v, magnitude);
    return magnitude;
}
korl_internal Korl_Math_V3f32 korl_math_v3f32_cross(Korl_Math_V3f32 vA, Korl_Math_V3f32 vB)
{
    Korl_Math_V3f32 result;
    result.x = vA.y*vB.z - vA.z*vB.y;
    result.y = vA.z*vB.x - vA.x*vB.z;
    result.z = vA.x*vB.y - vA.y*vB.x;
    return result;
}
korl_internal Korl_Math_V3f32 korl_math_v3f32_tripleProduct(Korl_Math_V3f32 vA, Korl_Math_V3f32 vB, Korl_Math_V3f32 vC)
{
    const Korl_Math_V3f32 temp = korl_math_v3f32_cross(vA, vB);
    return korl_math_v3f32_cross(temp, vC);
}
korl_internal f32 korl_math_v3f32_dot(Korl_Math_V3f32 vA, Korl_Math_V3f32 vB)
{
    return vA.elements[0] * vB.elements[0]
         + vA.elements[1] * vB.elements[1]
         + vA.elements[2] * vB.elements[2];
}
korl_internal f32 korl_math_v3f32_radiansBetween(Korl_Math_V3f32 vA, Korl_Math_V3f32 vB)
{
    return korl_math_arcCosine(korl_math_v3f32_dot(korl_math_v3f32_normal(vA), korl_math_v3f32_normal(vB)));
}
korl_internal Korl_Math_V3f32 korl_math_v3f32_project(Korl_Math_V3f32 vA, Korl_Math_V3f32 vB, bool vbIsNormalized)
{
    if(!vbIsNormalized)
        vB = korl_math_v3f32_normal(vB);
    const f32 scalarProjection = korl_math_v3f32_dot(vA, vB);
    return korl_math_v3f32_multiplyScalar(vB, scalarProjection);
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
korl_internal Korl_Math_V3f32 korl_math_v3f32_multiply(Korl_Math_V3f32 vA, Korl_Math_V3f32 vB)
{
    vA.elements[0] *= vB.elements[0];
    vA.elements[1] *= vB.elements[1];
    vA.elements[2] *= vB.elements[2];
    return vA;
}
korl_internal Korl_Math_V3f32 korl_math_v3f32_multiplyScalar(Korl_Math_V3f32 v, f32 scalar)
{
    v.elements[0] *= scalar;
    v.elements[1] *= scalar;
    v.elements[2] *= scalar;
    return v;
}
korl_internal Korl_Math_V3f32 korl_math_v3f32_divide(Korl_Math_V3f32 vA, Korl_Math_V3f32 vB)
{
    vA.elements[0] /= vB.elements[0];
    vA.elements[1] /= vB.elements[1];
    vA.elements[2] /= vB.elements[2];
    return vA;
}
korl_internal Korl_Math_V3f32 korl_math_v3f32_divideScalar(Korl_Math_V3f32 v, f32 scalar)
{
    v.elements[0] /= scalar;
    v.elements[1] /= scalar;
    v.elements[2] /= scalar;
    return v;
}
korl_internal void korl_math_v3f32_assignAdd(Korl_Math_V3f32*const vA, Korl_Math_V3f32 vB)
{
    vA->elements[0] += vB.elements[0];
    vA->elements[1] += vB.elements[1];
    vA->elements[2] += vB.elements[2];
}
korl_internal void korl_math_v3f32_assignAddScalar(Korl_Math_V3f32*const v, f32 scalar)
{
    v->elements[0] += scalar;
    v->elements[1] += scalar;
    v->elements[2] += scalar;
}
korl_internal void korl_math_v3f32_assignSubtract(Korl_Math_V3f32*const vA, Korl_Math_V3f32 vB)
{
    vA->elements[0] -= vB.elements[0];
    vA->elements[1] -= vB.elements[1];
    vA->elements[2] -= vB.elements[2];
}
korl_internal void korl_math_v3f32_assignSubtractScalar(Korl_Math_V3f32*const v, f32 scalar)
{
    v->elements[0] -= scalar;
    v->elements[1] -= scalar;
    v->elements[2] -= scalar;
}
korl_internal void korl_math_v3f32_assignMultiply(Korl_Math_V3f32*const vA, Korl_Math_V3f32 vB)
{
    vA->elements[0] *= vB.elements[0];
    vA->elements[1] *= vB.elements[1];
    vA->elements[2] *= vB.elements[2];
}
korl_internal void korl_math_v3f32_assignMultiplyScalar(Korl_Math_V3f32*const v, f32 scalar)
{
    v->elements[0] *= scalar;
    v->elements[1] *= scalar;
    v->elements[2] *= scalar;
}
korl_internal Korl_Math_V3f32 korl_math_v3f32_interpolateLinear(Korl_Math_V3f32 vFactorZero, Korl_Math_V3f32 vFactorOne, f32 factor)
{
    const f32 inverseFactor = 1.f - factor;
    for(u8 i = 0; i < korl_arraySize(vFactorZero.elements); i++)
        vFactorZero.elements[i] = inverseFactor * vFactorZero.elements[i] + factor * vFactorOne.elements[i];
    return vFactorZero;
}
korl_internal bool korl_math_v3f32_isNearlyZero(Korl_Math_V3f32 v)
{
    return korl_math_isNearlyZero(v.elements[0]) 
        && korl_math_isNearlyZero(v.elements[1]) 
        && korl_math_isNearlyZero(v.elements[2]);
}
#pragma warning(push)
#pragma warning(disable:4701)/* uninitialized local variable - trust me bro I know what I'm doing here */
korl_internal Korl_Math_V4u8 korl_math_v4u8_lerp(Korl_Math_V4u8 from, Korl_Math_V4u8 to, f32 factor)
{
    Korl_Math_V4u8 result;
    for(u8 i = 0; i < korl_arraySize(result.elements); i++)
        result.elements[i] = korl_math_round_f32_to_u8(korl_math_f32_lerp(from.elements[i], to.elements[i], factor));
    return result;
}
#pragma warning(pop)
korl_internal f32 korl_math_v4f32_magnitude(const Korl_Math_V4f32*const v)
{
    return korl_math_f32_squareRoot(korl_math_v4f32_magnitudeSquared(v));
}
korl_internal f32 korl_math_v4f32_magnitudeSquared(const Korl_Math_V4f32*const v)
{
    return korl_math_f32_power(v->elements[0], 2) 
         + korl_math_f32_power(v->elements[1], 2) 
         + korl_math_f32_power(v->elements[2], 2) 
         + korl_math_f32_power(v->elements[3], 2);
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
         + vA->elements[1] * vB->elements[1]
         + vA->elements[2] * vB->elements[2]
         + vA->elements[3] * vB->elements[3];
}
korl_internal Korl_Math_V4f32 korl_math_v4f32_add(Korl_Math_V4f32 vA, Korl_Math_V4f32 vB)
{
    for(u8 i = 0; i < korl_arraySize(vA.elements); i++)
        vA.elements[i] += vB.elements[i];
    return vA;
}
korl_internal Korl_Math_V4f32 korl_math_v4f32_addScalar(Korl_Math_V4f32 v, f32 scalar)
{
    for(u8 i = 0; i < korl_arraySize(v.elements); i++)
        v.elements[i] += scalar;
    return v;
}
korl_internal Korl_Math_V4f32 korl_math_v4f32_subtract(Korl_Math_V4f32 vA, Korl_Math_V4f32 vB)
{
    for(u8 i = 0; i < korl_arraySize(vA.elements); i++)
        vA.elements[i] -= vB.elements[i];
    return vA;
}
korl_internal Korl_Math_V4f32 korl_math_v4f32_subtractScalar(Korl_Math_V4f32 v, f32 scalar)
{
    for(u8 i = 0; i < korl_arraySize(v.elements); i++)
        v.elements[i] -= scalar;
    return v;
}
korl_internal Korl_Math_V4f32 korl_math_v4f32_multiply(Korl_Math_V4f32 vA, Korl_Math_V4f32 vB)
{
    for(u8 i = 0; i < korl_arraySize(vA.elements); i++)
        vA.elements[i] *= vB.elements[i];
    return vA;
}
korl_internal Korl_Math_V4f32 korl_math_v4f32_multiplyScalar(Korl_Math_V4f32 v, f32 scalar)
{
    for(u8 i = 0; i < korl_arraySize(v.elements); i++)
        v.elements[i] *= scalar;
    return v;
}
korl_internal Korl_Math_V4f32 korl_math_v4f32_divide(Korl_Math_V4f32 vA, Korl_Math_V4f32 vB)
{
    for(u8 i = 0; i < korl_arraySize(vA.elements); i++)
        vA.elements[i] /= vB.elements[i];
    return vA;
}
korl_internal Korl_Math_V4f32 korl_math_v4f32_divideScalar(Korl_Math_V4f32 v, f32 scalar)
{
    for(u8 i = 0; i < korl_arraySize(v.elements); i++)
        v.elements[i] /= scalar;
    return v;
}
korl_internal Korl_Math_V4f32* korl_math_v4f32_assignAdd(Korl_Math_V4f32*const vA, Korl_Math_V4f32 vB)
{
    for(u8 i = 0; i < korl_arraySize(vA->elements); i++)
        vA->elements[i] += vB.elements[i];
    return vA;
}
korl_internal Korl_Math_V4f32* korl_math_v4f32_assignAddScalar(Korl_Math_V4f32*const v, f32 scalar)
{
    for(u8 i = 0; i < korl_arraySize(v->elements); i++)
        v->elements[i] += scalar;
    return v;
}
korl_internal Korl_Math_V4f32* korl_math_v4f32_assignSubtract(Korl_Math_V4f32*const vA, Korl_Math_V4f32 vB)
{
    for(u8 i = 0; i < korl_arraySize(vA->elements); i++)
        vA->elements[i] -= vB.elements[i];
    return vA;
}
korl_internal Korl_Math_V4f32* korl_math_v4f32_assignSubtractScalar(Korl_Math_V4f32*const v, f32 scalar)
{
    for(u8 i = 0; i < korl_arraySize(v->elements); i++)
        v->elements[i] -= scalar;
    return v;
}
korl_internal Korl_Math_V4f32* korl_math_v4f32_assignMultiply(Korl_Math_V4f32*const vA, Korl_Math_V4f32 vB)
{
    for(u8 i = 0; i < korl_arraySize(vA->elements); i++)
        vA->elements[i] *= vB.elements[i];
    return vA;
}
korl_internal Korl_Math_V4f32* korl_math_v4f32_assignMultiplyScalar(Korl_Math_V4f32*const v, f32 scalar)
{
    for(u8 i = 0; i < korl_arraySize(v->elements); i++)
        v->elements[i] *= scalar;
    return v;
}
korl_internal Korl_Math_V4f32* korl_math_v4f32_assignDivide(Korl_Math_V4f32*const vA, Korl_Math_V4f32 vB)
{
    for(u8 i = 0; i < korl_arraySize(vA->elements); i++)
        vA->elements[i] /= vB.elements[i];
    return vA;
}
korl_internal Korl_Math_V4f32* korl_math_v4f32_assignDivideScalar(Korl_Math_V4f32*const v, f32 scalar)
{
    for(u8 i = 0; i < korl_arraySize(v->elements); i++)
        v->elements[i] /= scalar;
    return v;
}
korl_internal Korl_Math_V4f32 korl_math_v4f32_interpolateLinear(Korl_Math_V4f32 vFactorZero, Korl_Math_V4f32 vFactorOne, f32 factor)
{
    const f32 inverseFactor = 1.f - factor;
    for(u8 i = 0; i < korl_arraySize(vFactorZero.elements); i++)
        vFactorZero.elements[i] = inverseFactor * vFactorZero.elements[i] + factor * vFactorOne.elements[i];
    return vFactorZero;
}
korl_internal Korl_Math_Quaternion korl_math_quaternion_normal(Korl_Math_Quaternion q)
{
    const f32 magnitude = korl_math_v4f32_magnitude(&q.v4);
    q.v4 = korl_math_v4f32_normalKnownMagnitude(q.v4, magnitude);
    return q;
}
korl_internal f32 korl_math_quaternion_normalize(Korl_Math_Quaternion* q)
{
    const f32 magnitude = korl_math_v4f32_magnitude(&q->v4);
    q->v4 = korl_math_v4f32_normalKnownMagnitude(q->v4, magnitude);
    return magnitude;
}
korl_internal Korl_Math_Quaternion korl_math_quaternion_fromAxisRadians(Korl_Math_V3f32 axis, f32 radians, bool axisIsNormalized)
{
    korl_assert(!(   korl_math_isNearlyZero(axis.x)
                  && korl_math_isNearlyZero(axis.y)
                  && korl_math_isNearlyZero(axis.z)));// sanity check; we do not want to be making quaternions from NULL axes!
    if(!axisIsNormalized)
        axis = korl_math_v3f32_normal(axis);
    const f32 sine = korl_math_sine(radians/2);
    return korl_math_quaternion_make
        (sine * axis.x
        ,sine * axis.y
        ,sine * axis.z
        ,korl_math_cosine(radians/2));
}
korl_internal Korl_Math_Quaternion korl_math_quaternion_fromVector(Korl_Math_V3f32 forward, Korl_Math_V3f32 worldForward, Korl_Math_V3f32 worldUp)
{
    const f32 forwardMagnitude      = korl_math_v3f32_magnitude(&forward);
    const f32 worldForwardMagnitude = korl_math_v3f32_magnitude(&worldForward);
    const f32 worldUpMagnitude      = korl_math_v3f32_magnitude(&worldUp);
    korl_assert(!korl_math_isNearlyZero(forwardMagnitude));
    korl_assert(!korl_math_isNearlyZero(worldForwardMagnitude));
    korl_assert(!korl_math_isNearlyZero(worldUpMagnitude));
    forward      = korl_math_v3f32_normalKnownMagnitude(forward, forwardMagnitude);
    worldForward = korl_math_v3f32_normalKnownMagnitude(worldForward, worldForwardMagnitude);
    worldUp      = korl_math_v3f32_normalKnownMagnitude(worldUp, worldUpMagnitude);
    const Korl_Math_V3f32      forward_componentUp      = korl_math_v3f32_project(forward, worldUp, true);
    const Korl_Math_V3f32      forward_componentHorizon = korl_math_v3f32_subtract(forward, forward_componentUp);
    const Korl_Math_V3f32      yawAxis                  = korl_math_v3f32_cross(worldForward, forward_componentHorizon);
    const f32                  yawRadians               = korl_math_v3f32_radiansBetween(worldForward, forward_componentHorizon);
    const Korl_Math_Quaternion quaternionYaw            = korl_math_quaternion_fromAxisRadians(yawAxis, yawRadians, false);
    const Korl_Math_V3f32      worldForwardYawed        = korl_math_quaternion_transformV3f32(quaternionYaw, worldForward, false);
    const Korl_Math_V3f32      pitchAxis                = korl_math_v3f32_cross(worldForwardYawed, forward);
    const f32                  pitchRadians             = korl_math_v3f32_radiansBetween(worldForwardYawed, forward);
    const Korl_Math_Quaternion quaternionPitch          = korl_math_quaternion_fromAxisRadians(pitchAxis, pitchRadians, false);
    return korl_math_quaternion_multiply(quaternionYaw, quaternionPitch);
}
korl_internal Korl_Math_Quaternion korl_math_quaternion_multiply(Korl_Math_Quaternion qA, Korl_Math_Quaternion qB)
{
    return korl_math_quaternion_hamilton(qA, qB);
}
korl_internal Korl_Math_Quaternion korl_math_quaternion_hamilton(Korl_Math_Quaternion qA, Korl_Math_Quaternion qB)
{
    return korl_math_quaternion_make
        (qA.w*qB.x + qA.x*qB.w + qA.y*qB.z - qA.z*qB.y
        ,qA.w*qB.y - qA.x*qB.z + qA.y*qB.w + qA.z*qB.x
        ,qA.w*qB.z + qA.x*qB.y - qA.y*qB.x + qA.z*qB.w
        ,qA.w*qB.w - qA.x*qB.x - qA.y*qB.y - qA.z*qB.z);
}
korl_internal Korl_Math_Quaternion korl_math_quaternion_conjugate(Korl_Math_Quaternion q)
{
    return korl_math_quaternion_make(-q.x, -q.y, -q.z, q.w);
}
korl_internal Korl_Math_V2f32 korl_math_quaternion_transformV2f32(Korl_Math_Quaternion q, Korl_Math_V2f32 v, bool qIsNormalized)
{
    if(!qIsNormalized)
        q.v4 = korl_math_v4f32_normal(q.v4);
    q = korl_math_quaternion_hamilton(korl_math_quaternion_hamilton(q, korl_math_quaternion_make(v.x, v.y, 0.f, 0.f))
                                     ,korl_math_quaternion_conjugate(q));
    return q.v4.xy;
}
korl_internal Korl_Math_V3f32 korl_math_quaternion_transformV3f32(Korl_Math_Quaternion q, Korl_Math_V3f32 v, bool qIsNormalized)
{
    if(!qIsNormalized)
        q.v4 = korl_math_v4f32_normal(q.v4);
    q = korl_math_quaternion_hamilton(korl_math_quaternion_hamilton(q, korl_math_quaternion_make(v.x, v.y, v.z, 0.f))
                                     ,korl_math_quaternion_conjugate(q));
    return q.v4.xyz;
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
korl_internal void korl_math_m4f32_decompose(Korl_Math_M4f32 m, Korl_Math_V3f32* o_translation, Korl_Math_Quaternion* o_rotation, Korl_Math_V3f32* o_scale)
{
    /* derived from https://math.stackexchange.com/a/1463487 
        ASSUMPTION: matrix `m` only contains 0 or more of {rotation, translation, non-zero scaling}, which is fine for now */
    /* extract translation */
    *o_translation = korl_math_v3f32_make(m.r0c3, m.r1c3, m.r2c3);
    m.r0c3 = m.r1c3 = m.r2c3 = 0;
    /* extract scale */
    Korl_Math_V3f32 column0 = korl_math_v3f32_make(m.r0c0, m.r1c0, m.r2c0);
    Korl_Math_V3f32 column1 = korl_math_v3f32_make(m.r0c1, m.r1c1, m.r2c1);
    Korl_Math_V3f32 column2 = korl_math_v3f32_make(m.r0c2, m.r1c2, m.r2c2);
    *o_scale = korl_math_v3f32_make(korl_math_v3f32_magnitude(&column0)
                                   ,korl_math_v3f32_magnitude(&column1)
                                   ,korl_math_v3f32_magnitude(&column2));
    korl_assert(!korl_math_isNearlyZero(o_scale->x));
    korl_assert(!korl_math_isNearlyZero(o_scale->y));
    korl_assert(!korl_math_isNearlyZero(o_scale->z));
    /* extract rotation */
    m.r0c0 /= o_scale->x; m.r1c0 /= o_scale->x; m.r2c0 /= o_scale->x;
    m.r0c1 /= o_scale->y; m.r1c1 /= o_scale->y; m.r2c1 /= o_scale->y;
    m.r0c2 /= o_scale->z; m.r1c2 /= o_scale->z; m.r2c2 /= o_scale->z;
    // `m` is now a rotation matrix; we need only convert to a Quaternion now:
    // derived from https://danceswithcode.net/engineeringnotes/quaternions/quaternions.html
    const Korl_Math_V4f32 unsignedQuaternionComponents = korl_math_v4f32_make(korl_math_f32_squareRoot((1 + m.r0c0 + m.r1c1 + m.r2c2) / 4.f)// NOTE: elements[0] of this vector is the _real_ component of the final quaternion
                                                                             ,korl_math_f32_squareRoot((1 + m.r0c0 - m.r1c1 - m.r2c2) / 4.f)
                                                                             ,korl_math_f32_squareRoot((1 - m.r0c0 + m.r1c1 - m.r2c2) / 4.f)
                                                                             ,korl_math_f32_squareRoot((1 - m.r0c0 - m.r1c1 + m.r2c2) / 4.f));
    if(   unsignedQuaternionComponents.x > unsignedQuaternionComponents.y 
       && unsignedQuaternionComponents.x > unsignedQuaternionComponents.z 
       && unsignedQuaternionComponents.x > unsignedQuaternionComponents.w)// x component is the largest
        *o_rotation = korl_math_quaternion_make((m.r2c1 - m.r1c2) / (4 * unsignedQuaternionComponents.x)
                                               ,(m.r0c2 - m.r2c0) / (4 * unsignedQuaternionComponents.x)
                                               ,(m.r1c0 - m.r0c1) / (4 * unsignedQuaternionComponents.x)
                                               ,unsignedQuaternionComponents.x);
    else if(   unsignedQuaternionComponents.y > unsignedQuaternionComponents.x 
            && unsignedQuaternionComponents.y > unsignedQuaternionComponents.z 
            && unsignedQuaternionComponents.y > unsignedQuaternionComponents.w)// y component is the largest
        *o_rotation = korl_math_quaternion_make(unsignedQuaternionComponents.y
                                               ,(m.r0c1 + m.r1c0) / (4 * unsignedQuaternionComponents.y)
                                               ,(m.r0c2 + m.r2c0) / (4 * unsignedQuaternionComponents.y)
                                               ,(m.r2c1 - m.r1c2) / (4 * unsignedQuaternionComponents.y));
    else if(   unsignedQuaternionComponents.z > unsignedQuaternionComponents.x 
            && unsignedQuaternionComponents.z > unsignedQuaternionComponents.y 
            && unsignedQuaternionComponents.z > unsignedQuaternionComponents.w)// z component is the largest
        *o_rotation = korl_math_quaternion_make((m.r0c1 + m.r1c0) / (4 * unsignedQuaternionComponents.z)
                                               ,unsignedQuaternionComponents.z
                                               ,(m.r1c2 + m.r2c1) / (4 * unsignedQuaternionComponents.z)
                                               ,(m.r0c2 - m.r2c0) / (4 * unsignedQuaternionComponents.z));
    else// w component is the largest
    {
        *o_rotation = korl_math_quaternion_make((m.r0c2 + m.r2c0) / (4 * unsignedQuaternionComponents.w)
                                               ,(m.r1c2 + m.r2c1) / (4 * unsignedQuaternionComponents.w)
                                               ,unsignedQuaternionComponents.w
                                               ,(m.r1c0 - m.r0c1) / (4 * unsignedQuaternionComponents.w));
    }
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
korl_internal Korl_Math_M4f32 korl_math_m4f32_invert(const Korl_Math_M4f32*const m)
{
    Korl_Math_M4f32 result;
    /* implementation derived from MESA's GLU lib: 
        https://stackoverflow.com/a/1148405/4526664 */
    result.elements[ 0] =  m->elements[ 5] * m->elements[10] * m->elements[15] 
                        -  m->elements[ 5] * m->elements[11] * m->elements[14] 
                        -  m->elements[ 9] * m->elements[ 6] * m->elements[15] 
                        +  m->elements[ 9] * m->elements[ 7] * m->elements[14] 
                        +  m->elements[13] * m->elements[ 6] * m->elements[11] 
                        -  m->elements[13] * m->elements[ 7] * m->elements[10];
    result.elements[ 4] = -m->elements[ 4] * m->elements[10] * m->elements[15] 
                        +  m->elements[ 4] * m->elements[11] * m->elements[14] 
                        +  m->elements[ 8] * m->elements[ 6] * m->elements[15] 
                        -  m->elements[ 8] * m->elements[ 7] * m->elements[14] 
                        -  m->elements[12] * m->elements[ 6] * m->elements[11] 
                        +  m->elements[12] * m->elements[ 7] * m->elements[10];
    result.elements[ 8] =  m->elements[ 4] * m->elements[ 9] * m->elements[15] 
                        -  m->elements[ 4] * m->elements[11] * m->elements[13] 
                        -  m->elements[ 8] * m->elements[ 5] * m->elements[15] 
                        +  m->elements[ 8] * m->elements[ 7] * m->elements[13] 
                        +  m->elements[12] * m->elements[ 5] * m->elements[11] 
                        -  m->elements[12] * m->elements[ 7] * m->elements[ 9];
    result.elements[12] = -m->elements[ 4] * m->elements[ 9] * m->elements[14] 
                        +  m->elements[ 4] * m->elements[10] * m->elements[13] 
                        +  m->elements[ 8] * m->elements[ 5] * m->elements[14] 
                        -  m->elements[ 8] * m->elements[ 6] * m->elements[13] 
                        -  m->elements[12] * m->elements[ 5] * m->elements[10] 
                        +  m->elements[12] * m->elements[ 6] * m->elements[9];
    result.elements[ 1] = -m->elements[ 1] * m->elements[10] * m->elements[15] 
                        +  m->elements[ 1] * m->elements[11] * m->elements[14] 
                        +  m->elements[ 9] * m->elements[ 2] * m->elements[15] 
                        -  m->elements[ 9] * m->elements[ 3] * m->elements[14] 
                        -  m->elements[13] * m->elements[ 2] * m->elements[11] 
                        +  m->elements[13] * m->elements[ 3] * m->elements[10];
    result.elements[ 5] =  m->elements[ 0] * m->elements[10] * m->elements[15] 
                        -  m->elements[ 0] * m->elements[11] * m->elements[14] 
                        -  m->elements[ 8] * m->elements[ 2] * m->elements[15] 
                        +  m->elements[ 8] * m->elements[ 3] * m->elements[14] 
                        +  m->elements[12] * m->elements[ 2] * m->elements[11] 
                        -  m->elements[12] * m->elements[ 3] * m->elements[10];
    result.elements[ 9] = -m->elements[ 0] * m->elements[ 9] * m->elements[15] 
                        +  m->elements[ 0] * m->elements[11] * m->elements[13] 
                        +  m->elements[ 8] * m->elements[ 1] * m->elements[15] 
                        -  m->elements[ 8] * m->elements[ 3] * m->elements[13] 
                        -  m->elements[12] * m->elements[ 1] * m->elements[11] 
                        +  m->elements[12] * m->elements[ 3] * m->elements[ 9];
    result.elements[13] =  m->elements[ 0] * m->elements[ 9] * m->elements[14] 
                        -  m->elements[ 0] * m->elements[10] * m->elements[13] 
                        -  m->elements[ 8] * m->elements[ 1] * m->elements[14] 
                        +  m->elements[ 8] * m->elements[ 2] * m->elements[13] 
                        +  m->elements[12] * m->elements[ 1] * m->elements[10] 
                        -  m->elements[12] * m->elements[ 2] * m->elements[ 9];
    result.elements[ 2] =  m->elements[ 1] * m->elements[ 6] * m->elements[15] 
                        -  m->elements[ 1] * m->elements[ 7] * m->elements[14] 
                        -  m->elements[ 5] * m->elements[ 2] * m->elements[15] 
                        +  m->elements[ 5] * m->elements[ 3] * m->elements[14] 
                        +  m->elements[13] * m->elements[ 2] * m->elements[ 7] 
                        -  m->elements[13] * m->elements[ 3] * m->elements[ 6];
    result.elements[ 6] = -m->elements[ 0] * m->elements[ 6] * m->elements[15] 
                        +  m->elements[ 0] * m->elements[ 7] * m->elements[14] 
                        +  m->elements[ 4] * m->elements[ 2] * m->elements[15] 
                        -  m->elements[ 4] * m->elements[ 3] * m->elements[14] 
                        -  m->elements[12] * m->elements[ 2] * m->elements[ 7] 
                        +  m->elements[12] * m->elements[ 3] * m->elements[ 6];
    result.elements[10] =  m->elements[ 0] * m->elements[ 5] * m->elements[15] 
                        -  m->elements[ 0] * m->elements[ 7] * m->elements[13] 
                        -  m->elements[ 4] * m->elements[ 1] * m->elements[15] 
                        +  m->elements[ 4] * m->elements[ 3] * m->elements[13] 
                        +  m->elements[12] * m->elements[ 1] * m->elements[ 7] 
                        -  m->elements[12] * m->elements[ 3] * m->elements[ 5];
    result.elements[14] = -m->elements[ 0] * m->elements[ 5] * m->elements[14] 
                        +  m->elements[ 0] * m->elements[ 6] * m->elements[13] 
                        +  m->elements[ 4] * m->elements[ 1] * m->elements[14] 
                        -  m->elements[ 4] * m->elements[ 2] * m->elements[13] 
                        -  m->elements[12] * m->elements[ 1] * m->elements[ 6] 
                        +  m->elements[12] * m->elements[ 2] * m->elements[ 5];
    result.elements[ 3] = -m->elements[ 1] * m->elements[ 6] * m->elements[11] 
                        +  m->elements[ 1] * m->elements[ 7] * m->elements[10] 
                        +  m->elements[ 5] * m->elements[ 2] * m->elements[11] 
                        -  m->elements[ 5] * m->elements[ 3] * m->elements[10] 
                        -  m->elements[ 9] * m->elements[ 2] * m->elements[ 7] 
                        +  m->elements[ 9] * m->elements[ 3] * m->elements[ 6];
    result.elements[ 7] =  m->elements[ 0] * m->elements[ 6] * m->elements[11] 
                        -  m->elements[ 0] * m->elements[ 7] * m->elements[10] 
                        -  m->elements[ 4] * m->elements[ 2] * m->elements[11] 
                        +  m->elements[ 4] * m->elements[ 3] * m->elements[10] 
                        +  m->elements[ 8] * m->elements[ 2] * m->elements[ 7] 
                        -  m->elements[ 8] * m->elements[ 3] * m->elements[ 6];
    result.elements[11] = -m->elements[ 0] * m->elements[ 5] * m->elements[11] 
                        +  m->elements[ 0] * m->elements[ 7] * m->elements[ 9] 
                        +  m->elements[ 4] * m->elements[ 1] * m->elements[11] 
                        -  m->elements[ 4] * m->elements[ 3] * m->elements[ 9] 
                        -  m->elements[ 8] * m->elements[ 1] * m->elements[ 7] 
                        +  m->elements[ 8] * m->elements[ 3] * m->elements[ 5];
    result.elements[15] =  m->elements[ 0] * m->elements[ 5] * m->elements[10] 
                        -  m->elements[ 0] * m->elements[ 6] * m->elements[ 9] 
                        -  m->elements[ 4] * m->elements[ 1] * m->elements[10] 
                        +  m->elements[ 4] * m->elements[ 2] * m->elements[ 9] 
                        +  m->elements[ 8] * m->elements[ 1] * m->elements[ 6] 
                        -  m->elements[ 8] * m->elements[ 2] * m->elements[ 5];
    f32 det = m->elements[0] * result.elements[ 0] 
            + m->elements[1] * result.elements[ 4] 
            + m->elements[2] * result.elements[ 8] 
            + m->elements[3] * result.elements[12];
    if (korl_math_isNearlyEqualEpsilon(det, 0.f, 1e-10f))
    {
        result.r0c0 = korl_math_f32_quietNan();
        return result;
    }
    det = 1.0f / det;
    for (u8 i = 0; i < 16; i++)
        result.elements[i] *= det;
    return result;
}
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
korl_internal Korl_Math_V3f32 korl_math_m4f32_multiplyV3f32(const Korl_Math_M4f32*const m, Korl_Math_V3f32 v)
{
    const Korl_Math_V4f32 vHomogeneous = korl_math_v4f32_make(v.x, v.y, v.z, 1.f);
    Korl_Math_V4f32 result;
    for(u$ r = 0; r < korl_arraySize(m->rows); r++)
        result.elements[r] = korl_math_v4f32_dot(&m->rows[r], &vHomogeneous);
    result.xyz = korl_math_v3f32_multiplyScalar(result.xyz, 1.f / result.w);// "normalize" the 3D component; transform back to homogeneous coordinates, where w == 1
    return result.xyz;
}
korl_internal Korl_Math_V4f32 korl_math_m4f32_multiplyV4f32(const Korl_Math_M4f32*const m, const Korl_Math_V4f32*const v)
{
    Korl_Math_V4f32 result;
    for(u$ r = 0; r < korl_arraySize(m->rows); r++)
        result.elements[r] = korl_math_v4f32_dot(&m->rows[r], v);
    return result;
}
#pragma warning(pop)
korl_internal Korl_Math_V4f32 korl_math_m4f32_multiplyV4f32Copy(const Korl_Math_M4f32*const m, Korl_Math_V4f32 v)
{
    return korl_math_m4f32_multiplyV4f32(m, &v);
}
korl_internal Korl_Math_M4f32 korl_math_m4f32_projectionFov(f32 verticalFovDegrees, f32 viewportWidthOverHeight, f32 clipNear, f32 clipFar)
{
    KORL_MATH_ZERO_STACK(Korl_Math_M4f32, result);
    KORL_MATH_ASSERT(!korl_math_isNearlyZero(verticalFovDegrees));
    KORL_MATH_ASSERT(!korl_math_isNearlyEqual(clipNear, clipFar) && clipNear < clipFar);
    const f32 verticalFovRadians = verticalFovDegrees*KORL_PI32/180.f;
    const f32 tanHalfFov = korl_math_tangent(verticalFovRadians / 2.f);
    /* Good sources for how to derive this matrix:
        http://ogldev.org/www/tutorial12/tutorial12.html
        http://www.songho.ca/opengl/gl_projectionmatrix.html */
    /* transforms eye-space into clip-space with the following properties:
        - mapped to [0,1], because Vulkan default requires VkViewport.min/maxDepth 
          values to lie within the range [0,1] (unless the extension 
          `VK_EXT_depth_range_unrestricted` is used, but who wants to do that?)
        - clipNear maps to Z==1 in clip-space
        - clipFar  maps to Z==0 in clip-space 
        For the reasoning why we want this clip-space mapping, see: https://developer.nvidia.com/content/depth-precision-visualized 
        In addition, korl-vulkan is now configured to require this clip-space mapping! */
    result.r0c0 = 1.f / (tanHalfFov * viewportWidthOverHeight);
    result.r1c1 = 1.f /  tanHalfFov;
    result.r2c2 =              clipNear / (clipNear - clipFar);
    result.r2c3 = -(clipNear * clipFar) / (clipNear - clipFar);
    result.r3c2 = 1;
    return result;
}
korl_internal Korl_Math_M4f32 korl_math_m4f32_projectionOrthographic(f32 xMin, f32 xMax, f32 yMin, f32 yMax, f32 zMin, f32 zMax)
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
korl_internal Korl_Math_M4f32 korl_math_m4f32_lookAt(const Korl_Math_V3f32*const positionEye, const Korl_Math_V3f32*const positionTarget, const Korl_Math_V3f32*const worldUpNormal)
{
    //KORL-PERFORMANCE-000-000-006
    /* sanity check to ensure that worldUpNormal is, in fact, normalized!  
        (because this is just a sanity check, it doesn't have to be too strict) */
    KORL_MATH_ASSERT(korl_math_isNearlyEqualEpsilon(korl_math_v3f32_magnitudeSquared(worldUpNormal), 1, 1e-2f));
    Korl_Math_M4f32 result = KORL_MATH_M4F32_IDENTITY;
    /* Even though this author uses "column-major" memory layout for matrices 
        for some reason, which makes everything really fucking confusing most of 
        the time, this still does a good job explaining a useful implementation 
        of the view matrix: https://www.3dgep.com/understanding-the-view-matrix/ */
    /* our camera eye-space will be left-handed, with the camera looking down 
        the +Z axis, and +Y being UP */
    const Korl_Math_V3f32 cameraZ = korl_math_v3f32_normal(korl_math_v3f32_subtract(*positionTarget, *positionEye));
    const Korl_Math_V3f32 cameraX = korl_math_v3f32_cross(cameraZ, *worldUpNormal);
    const Korl_Math_V3f32 cameraY = korl_math_v3f32_cross(cameraZ, cameraX);
    /* the view matrix is simply the inverse of the camera transform, and since 
        the camera transform is orthonormal, we can simply transpose the 
        elements of the rotation matrix */
    /* additionally, we can combine the inverse camera translation into the same 
        matrix as the inverse rotation by manually doing the row-column dot 
        products ourselves, and putting them in the 4th column */
    result.r0c0 = cameraX.x; result.r0c1 = cameraX.y; result.r0c2 = cameraX.z; result.r0c3 = -korl_math_v3f32_dot(cameraX, *positionEye);
    result.r1c0 = cameraY.x; result.r1c1 = cameraY.y; result.r1c2 = cameraY.z; result.r1c3 = -korl_math_v3f32_dot(cameraY, *positionEye);
    result.r2c0 = cameraZ.x; result.r2c1 = cameraZ.y; result.r2c2 = cameraZ.z; result.r2c3 = -korl_math_v3f32_dot(cameraZ, *positionEye);
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
korl_internal Korl_Math_Aabb2f32 korl_math_aabb2f32_fromPointsV2(const Korl_Math_V2f32* points, u$ pointsSize)
{
    Korl_Math_Aabb2f32 result = KORL_MATH_AABB2F32_EMPTY;
    for(u$ i = 0; i < pointsSize; i++)
    {
        KORL_MATH_ASSIGN_CLAMP_MAX(result.min.x, points[i].x);
        KORL_MATH_ASSIGN_CLAMP_MAX(result.min.y, points[i].y);
        KORL_MATH_ASSIGN_CLAMP_MIN(result.max.x, points[i].x);
        KORL_MATH_ASSIGN_CLAMP_MIN(result.max.y, points[i].y);
    }
    return result;
}
korl_internal Korl_Math_Aabb2f32 korl_math_aabb2f32_fromExpanded(Korl_Math_Aabb2f32 aabb, f32 expandX, f32 expandY)
{
    const Korl_Math_V2f32 expandV2 = korl_math_v2f32_make(expandX, expandY);
    return KORL_STRUCT_INITIALIZE(Korl_Math_Aabb2f32){.min = korl_math_v2f32_subtract(aabb.min, expandV2), 
                                                      .max = korl_math_v2f32_add     (aabb.max, expandV2)};
}
korl_internal Korl_Math_Aabb2f32 korl_math_aabb2f32_fromExpandedV2(Korl_Math_V2f32 v, f32 expandX, f32 expandY)
{
    const Korl_Math_V2f32 expandV2 = korl_math_v2f32_make(expandX, expandY);
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
    Korl_Math_V2f32 result = korl_math_v2f32_subtract(aabb.max, aabb.min);
    KORL_MATH_ASSIGN_CLAMP_MIN(result.x, 0);// if aabb is invalid in this dimension, the size is 0
    KORL_MATH_ASSIGN_CLAMP_MIN(result.y, 0);// if aabb is invalid in this dimension, the size is 0
    return result;
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
korl_internal Korl_Math_Aabb2f32 korl_math_aabb2f32_intersect(Korl_Math_Aabb2f32 aabbA, Korl_Math_Aabb2f32 aabbB)
{
    Korl_Math_Aabb2f32 result;
    result.min.x = KORL_MATH_MAX(aabbA.min.x, aabbB.min.x);
    result.min.y = KORL_MATH_MAX(aabbA.min.y, aabbB.min.y);
    result.max.x = KORL_MATH_MIN(aabbA.max.x, aabbB.max.x);
    result.max.y = KORL_MATH_MIN(aabbA.max.y, aabbB.max.y);
    return result;
}
korl_internal bool korl_math_aabb2f32_areIntersecting(Korl_Math_Aabb2f32 aabbA, Korl_Math_Aabb2f32 aabbB)
{
    return KORL_MATH_MAX(aabbA.min.x, aabbB.min.x) <= KORL_MATH_MIN(aabbA.max.x, aabbB.max.x)
        && KORL_MATH_MAX(aabbA.min.y, aabbB.min.y) <= KORL_MATH_MIN(aabbA.max.y, aabbB.max.y);
}
korl_internal void korl_math_aabb2f32_addPoint(Korl_Math_Aabb2f32*const aabb, f32* point2d)
{
    KORL_MATH_ASSIGN_CLAMP_MAX(aabb->min.x, point2d[0]);
    KORL_MATH_ASSIGN_CLAMP_MIN(aabb->max.x, point2d[0]);
    KORL_MATH_ASSIGN_CLAMP_MAX(aabb->min.y, point2d[1]);
    KORL_MATH_ASSIGN_CLAMP_MIN(aabb->max.y, point2d[1]);
}
korl_internal void korl_math_aabb2f32_addPointV2(Korl_Math_Aabb2f32*const aabb, Korl_Math_V2f32 point)
{
    KORL_MATH_ASSIGN_CLAMP_MAX(aabb->min.x, point.x);
    KORL_MATH_ASSIGN_CLAMP_MIN(aabb->max.x, point.x);
    KORL_MATH_ASSIGN_CLAMP_MAX(aabb->min.y, point.y);
    KORL_MATH_ASSIGN_CLAMP_MIN(aabb->max.y, point.y);
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
korl_internal Korl_Math_Aabb3f32 korl_math_aabb3f32_fromExpandedV3(Korl_Math_V3f32 v, Korl_Math_V3f32 vExpansion)
{
    return KORL_STRUCT_INITIALIZE(Korl_Math_Aabb3f32){.min = korl_math_v3f32_subtract(v, vExpansion), 
                                                      .max = korl_math_v3f32_add     (v, vExpansion)};
}
korl_internal void korl_math_aabb3f32_expand(Korl_Math_Aabb3f32*const aabb, f32 expandXYZ)
{
    korl_math_v3f32_assignSubtractScalar(&aabb->min, expandXYZ);
    korl_math_v3f32_assignAddScalar     (&aabb->max, expandXYZ);
}
korl_internal Korl_Math_V3f32 korl_math_aabb3f32_size(Korl_Math_Aabb3f32 aabb)
{
    Korl_Math_V3f32 result = korl_math_v3f32_subtract(aabb.max, aabb.min);
    KORL_MATH_ASSIGN_CLAMP_MIN(result.x, 0);// if aabb is invalid in this dimension, the size is 0
    KORL_MATH_ASSIGN_CLAMP_MIN(result.y, 0);// if aabb is invalid in this dimension, the size is 0
    KORL_MATH_ASSIGN_CLAMP_MIN(result.z, 0);// if aabb is invalid in this dimension, the size is 0
    return result;
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
korl_internal bool korl_math_aabb3f32_areIntersecting(Korl_Math_Aabb3f32 aabbA, Korl_Math_Aabb3f32 aabbB)
{
    return KORL_MATH_MAX(aabbA.min.x, aabbB.min.x) <= KORL_MATH_MIN(aabbA.max.x, aabbB.max.x)
        && KORL_MATH_MAX(aabbA.min.y, aabbB.min.y) <= KORL_MATH_MIN(aabbA.max.y, aabbB.max.y)
        && KORL_MATH_MAX(aabbA.min.z, aabbB.min.z) <= KORL_MATH_MIN(aabbA.max.z, aabbB.max.z);
}
korl_internal void korl_math_aabb3f32_addPoint(Korl_Math_Aabb3f32*const aabb, f32* point3d)
{
    for(u8 i = 0; i < korl_arraySize(aabb->min.elements); i++)
    {
        KORL_MATH_ASSIGN_CLAMP_MAX(aabb->min.elements[i], point3d[i]);
        KORL_MATH_ASSIGN_CLAMP_MIN(aabb->max.elements[i], point3d[i]);
    }
}
korl_internal void korl_math_aabb3f32_addPointV3(Korl_Math_Aabb3f32*const aabb, Korl_Math_V3f32 point)
{
    for(u8 i = 0; i < korl_arraySize(point.elements); i++)
    {
        KORL_MATH_ASSIGN_CLAMP_MAX(aabb->min.elements[i], point.elements[i]);
        KORL_MATH_ASSIGN_CLAMP_MIN(aabb->max.elements[i], point.elements[i]);
    }
}
korl_internal Korl_Math_Transform3d korl_math_transform3d_identity(void)
{
    return KORL_STRUCT_INITIALIZE(Korl_Math_Transform3d){._m4f32          = KORL_MATH_M4F32_IDENTITY
                                                        ,._versor         = KORL_MATH_QUATERNION_IDENTITY
                                                        ,._position       = KORL_MATH_V3F32_ZERO
                                                        ,._scale          = KORL_MATH_V3F32_ONE
                                                        ,._m4f32IsUpdated = true};
}
korl_internal Korl_Math_Transform3d korl_math_transform3d_rotateTranslate(Korl_Math_Quaternion versor, Korl_Math_V3f32 position)
{
    return KORL_STRUCT_INITIALIZE(Korl_Math_Transform3d){._m4f32          = korl_math_makeM4f32_rotateTranslate(versor, position)
                                                        ,._versor         = versor
                                                        ,._position       = position
                                                        ,._scale          = KORL_MATH_V3F32_ONE
                                                        ,._m4f32IsUpdated = true};
}
korl_internal Korl_Math_Transform3d korl_math_transform3d_rotateScaleTranslate(Korl_Math_Quaternion versor, Korl_Math_V3f32 scale, Korl_Math_V3f32 position)
{
    return KORL_STRUCT_INITIALIZE(Korl_Math_Transform3d){._m4f32          = korl_math_makeM4f32_rotateScaleTranslate(versor, scale, position)
                                                        ,._versor         = versor
                                                        ,._position       = position
                                                        ,._scale          = scale
                                                        ,._m4f32IsUpdated = true};
}
korl_internal void korl_math_transform3d_setPosition(Korl_Math_Transform3d* context, Korl_Math_V3f32 position)
{
    KORL_STATIC_ASSERT(korl_arraySize(position.elements) == korl_arraySize(context->_position.elements));
    for(u8 i = 0; context->_m4f32IsUpdated && i < korl_arraySize(context->_position.elements); i++)
        if(context->_position.elements[i] != position.elements[i])
            context->_m4f32IsUpdated = false;
    context->_position = position;
}
korl_internal void korl_math_transform3d_move(Korl_Math_Transform3d* context, Korl_Math_V3f32 positionDelta)
{
    KORL_STATIC_ASSERT(korl_arraySize(positionDelta.elements) == korl_arraySize(context->_position.elements));
    for(u8 i = 0; context->_m4f32IsUpdated && i < korl_arraySize(context->_position.elements); i++)
        if(positionDelta.elements[i] != 0.f)
            context->_m4f32IsUpdated = false;
    korl_math_v3f32_assignAdd(&context->_position, positionDelta);
}
korl_internal void korl_math_transform3d_setVersor(Korl_Math_Transform3d* context, Korl_Math_Quaternion versor)
{
    KORL_STATIC_ASSERT(korl_arraySize(versor.elements) == korl_arraySize(context->_versor.elements));
    for(u8 i = 0; context->_m4f32IsUpdated && i < korl_arraySize(context->_versor.elements); i++)
        if(context->_versor.elements[i] != versor.elements[i])
            context->_m4f32IsUpdated = false;
    context->_versor = versor;
}
korl_internal void korl_math_transform3d_setScale(Korl_Math_Transform3d* context, Korl_Math_V3f32 scale)
{
    KORL_STATIC_ASSERT(korl_arraySize(scale.elements) == korl_arraySize(context->_scale.elements));
    for(u8 i = 0; context->_m4f32IsUpdated && i < korl_arraySize(context->_scale.elements); i++)
        if(context->_scale.elements[i] != scale.elements[i])
            context->_m4f32IsUpdated = false;
    context->_scale = scale;
}
korl_internal void korl_math_transform3d_setM4f32(Korl_Math_Transform3d* context, Korl_Math_M4f32 m4f32)
{
    korl_math_m4f32_decompose(m4f32, &context->_position, &context->_versor, &context->_scale);
    context->_m4f32          = m4f32;
    context->_m4f32IsUpdated = true;
}
korl_internal void korl_math_transform3d_updateM4f32(Korl_Math_Transform3d* context)
{
    if(context->_m4f32IsUpdated)
        return;
    context->_m4f32          = korl_math_makeM4f32_rotateScaleTranslate(context->_versor, context->_scale, context->_position);
    context->_m4f32IsUpdated = true;
}
korl_internal Korl_Math_M4f32 korl_math_transform3d_m4f32(Korl_Math_Transform3d* context)
{
    if(!context->_m4f32IsUpdated)
    {
        context->_m4f32          = korl_math_makeM4f32_rotateScaleTranslate(context->_versor, context->_scale, context->_position);
        context->_m4f32IsUpdated = true;
    }
    return context->_m4f32;
}
korl_internal Korl_Math_TriangleMesh korl_math_triangleMesh_create(Korl_Memory_AllocatorHandle allocator)
{
    KORL_ZERO_STACK(Korl_Math_TriangleMesh, result);
    result.allocator = allocator;
    result.aabb      = KORL_MATH_AABB3F32_EMPTY;
    mcarrsetcap(KORL_STB_DS_MC_CAST(allocator), result.stbDaVertices, 128);
    mcarrsetcap(KORL_STB_DS_MC_CAST(allocator), result.stbDaIndices , 128);
    return result;
}
korl_internal void korl_math_triangleMesh_destroy(Korl_Math_TriangleMesh* context)
{
    mcarrfree(KORL_STB_DS_MC_CAST(context->allocator), context->stbDaVertices);
    mcarrfree(KORL_STB_DS_MC_CAST(context->allocator), context->stbDaIndices);
    korl_memory_zero(context, sizeof(*context));
}
korl_internal void korl_math_triangleMesh_collectDefragmentPointers(Korl_Math_TriangleMesh* context, void* stbDaMemoryContext, Korl_Heap_DefragmentPointer** pStbDaDefragmentPointers, void* parent)
{
    KORL_MEMORY_STB_DA_DEFRAGMENT_STB_ARRAY_CHILD(stbDaMemoryContext, *pStbDaDefragmentPointers, context->stbDaVertices, parent);
    KORL_MEMORY_STB_DA_DEFRAGMENT_STB_ARRAY_CHILD(stbDaMemoryContext, *pStbDaDefragmentPointers, context->stbDaIndices , parent);
}
korl_internal void korl_math_triangleMesh_add(Korl_Math_TriangleMesh* context, const Korl_Math_V3f32* vertices, u32 verticesSize)
{
    korl_assert(verticesSize % 3 == 0);// triangles must obviously have 3 vertices
    const u$ startIndexVertices = arrlenu(context->stbDaVertices);
    const u$ startIndexIndices  = arrlenu(context->stbDaIndices);
    mcarrsetlen(KORL_STB_DS_MC_CAST(context->allocator), context->stbDaVertices, startIndexVertices + verticesSize);
    mcarrsetlen(KORL_STB_DS_MC_CAST(context->allocator), context->stbDaIndices , startIndexIndices  + verticesSize / 3);
    korl_memory_copy(context->stbDaVertices + startIndexVertices, vertices, verticesSize * sizeof(*vertices));
    for(u32 v = 0; v < verticesSize; v++)
    {
        korl_math_aabb3f32_addPointV3(&context->aabb, vertices[v]);
        context->stbDaIndices[startIndexIndices + v] = korl_checkCast_u$_to_u32(startIndexVertices + v);
    }
}
korl_internal void korl_math_triangleMesh_addIndexed(Korl_Math_TriangleMesh* context, const Korl_Math_V3f32* vertices, u32 verticesSize, const u32* indices, u32 indicesSize)
{
    const u$ startIndexVertices = arrlenu(context->stbDaVertices);
    const u$ startIndexIndices  = arrlenu(context->stbDaIndices);
    mcarrsetlen(KORL_STB_DS_MC_CAST(context->allocator), context->stbDaVertices, startIndexVertices + verticesSize);
    mcarrsetlen(KORL_STB_DS_MC_CAST(context->allocator), context->stbDaIndices , startIndexIndices  + indicesSize);
    korl_memory_copy(context->stbDaVertices + startIndexVertices, vertices, verticesSize * sizeof(*vertices));
    for(u32 v = 0; v < verticesSize; v++)
        korl_math_aabb3f32_addPointV3(&context->aabb, vertices[v]);
    for(u32 i = 0; i < indicesSize; i++)
        context->stbDaIndices[startIndexIndices + i] = korl_checkCast_u$_to_u32(startIndexVertices + indices[i]);
}
korl_internal f32 korl_math_collide_ray_plane(const Korl_Math_V3f32 rayOrigin, const Korl_Math_V3f32 rayNormal
                                             ,const Korl_Math_V3f32 planeNormal, f32 planeDistanceFromOrigin
                                             ,bool cullPlaneBackFace)
{
    KORL_MATH_ASSERT(korl_math_isNearlyEqual(korl_math_v3f32_magnitudeSquared(&rayNormal  ), 1));
    KORL_MATH_ASSERT(korl_math_isNearlyEqual(korl_math_v3f32_magnitudeSquared(&planeNormal), 1));
    const Korl_Math_V3f32 planeOrigin = korl_math_v3f32_multiplyScalar(planeNormal, planeDistanceFromOrigin);
    const f32             denominator = korl_math_v3f32_dot(rayNormal, planeNormal);
    if(korl_math_isNearlyZero(denominator))
    /* the ray is PERPENDICULAR to the plane; no real solution */
    {
        /* test if the ray is co-planar with the plane */
        const Korl_Math_V3f32 rayOriginOntoPlane = korl_math_v3f32_project(rayOrigin, planeOrigin, false);
        if(    korl_math_isNearlyEqual(rayOriginOntoPlane.x, rayOrigin.x)
            && korl_math_isNearlyEqual(rayOriginOntoPlane.y, rayOrigin.y)
            && korl_math_isNearlyEqual(rayOriginOntoPlane.z, rayOrigin.z))
            return korl_math_f32_infinity();
        /* otherwise, they will never intersect */
        return korl_math_f32_quietNan();
    }
    if(cullPlaneBackFace && denominator/*rayNormal dot planeNormal*/ >= 0)
    /* if we're colliding with the BACK of the plane (opposite side that the 
        normal is facing) and the caller doesn't want these collisions, then the 
        collision fails here */
        return korl_math_f32_quietNan();
    const f32 result = korl_math_v3f32_dot(korl_math_v3f32_subtract(planeOrigin, rayOrigin), planeNormal) / denominator;
    /* if the result is negative, then we are colliding with the ray's line, NOT the ray itself! */
    if(result < 0)
        return korl_math_f32_quietNan();
    return result;
}
korl_internal f32 korl_math_collide_ray_sphere(const Korl_Math_V3f32 rayOrigin, const Korl_Math_V3f32 rayNormal
                                              ,const Korl_Math_V3f32 sphereOrigin, f32 sphereRadius)
{
    /* analytic solution.  Source: https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-sphere-intersection */
    const Korl_Math_V3f32 l = korl_math_v3f32_subtract(rayOrigin, sphereOrigin);
    const f32             a = korl_math_v3f32_dot(rayNormal, rayNormal);
    const f32             b = 2 * korl_math_v3f32_dot(rayNormal, l);
    const f32             c = korl_math_v3f32_dot(l, l) - (sphereRadius * sphereRadius);
    f32 analyticSolutions[2];
    const u8 analyticSolutionCount = korl_math_solveQuadraticReal(a, b, c, analyticSolutions);
    /* no quadratic solutions means the ray's LINE never collides with the sphere */
    if(analyticSolutionCount == 0)
        return korl_math_f32_quietNan();
    /* rare case: the ray intersects the sphere at a SINGLE point */
    if(analyticSolutionCount == 1)
        if(analyticSolutions[0] < 0)
            return korl_math_f32_quietNan();/* intersection is on the wrong side of the ray */
        else
            return analyticSolutions[0];
    /* if the first solution is < 0, the sphere either intersects the ray's 
        origin, or the sphere is colliding with the ray's line BEHIND the ray 
        (no collision with the ray itself) */
    KORL_MATH_ASSERT(analyticSolutionCount == 2);
    if(analyticSolutions[0] < 0)
        if(analyticSolutions[1] < 0)
            return korl_math_f32_quietNan();// collision with line behind the ray
        else
            return 0;// collision with ray origin
    else
        return analyticSolutions[0];// standard collision with ray at some distance
}
#ifdef __cplusplus
korl_internal Korl_Math_V2u32 operator+(Korl_Math_V2u32 v, u32 scalar)
{
    return korl_math_v2u32_addScalar(v, scalar);
}
korl_internal Korl_Math_V2u32 operator/(Korl_Math_V2u32 vNumerator, Korl_Math_V2u32 vDenominator)
{
    return korl_math_v2u32_divide(vNumerator, vDenominator);
}
korl_internal Korl_Math_V2u32& operator/=(Korl_Math_V2u32& vA, Korl_Math_V2u32 vB)
{
    vA = korl_math_v2u32_divide(vA, vB);
    return vA;
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
korl_internal Korl_Math_V2f32 operator*(Korl_Math_V2f32 vA, Korl_Math_V2f32 vB)
{
    return korl_math_v2f32_multiply(vA, vB);
}
korl_internal Korl_Math_V2f32 operator*(Korl_Math_V2f32 v, f32 scalar)
{
    return korl_math_v2f32_multiplyScalar(v, scalar);
}
korl_internal Korl_Math_V2f32 operator*(f32 scalar, Korl_Math_V2f32 v)
{
    return korl_math_v2f32_multiplyScalar(v, scalar);
}
korl_internal Korl_Math_V2f32& operator*=(Korl_Math_V2f32& v, f32 scalar)
{
    korl_math_v2f32_assignMultiplyScalar(&v, scalar);
    return v;
}
korl_internal Korl_Math_V2f32 operator/(Korl_Math_V2f32 v, f32 scalar)
{
    return korl_math_v2f32_divideScalar(v, scalar);
}
korl_internal Korl_Math_V2f32& operator/=(Korl_Math_V2f32& v, f32 scalar)
{
    v = korl_math_v2f32_divideScalar(v, scalar);
    return v;
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
korl_internal Korl_Math_V2f32& operator*=(Korl_Math_V2f32& vA, Korl_Math_V2f32 vB)
{
    vA = korl_math_v2f32_multiply(vA, vB);
    return vA;
}
korl_internal Korl_Math_V3f32 operator+(Korl_Math_V3f32 vA, Korl_Math_V3f32 vB)
{
    return korl_math_v3f32_add(vA, vB);
}
korl_internal Korl_Math_V3f32 operator-(Korl_Math_V3f32 v)
{
    return korl_math_v3f32_multiplyScalar(v, -1);
}
korl_internal Korl_Math_V3f32 operator-(Korl_Math_V3f32 vA, Korl_Math_V3f32 vB)
{
    return korl_math_v3f32_subtract(vA, vB);
}
korl_internal Korl_Math_V3f32 operator*(Korl_Math_V3f32 v, f32 scalar)
{
    return korl_math_v3f32_multiplyScalar(v, scalar);
}
korl_internal Korl_Math_V3f32 operator*(f32 scalar, Korl_Math_V3f32 v)
{
    return korl_math_v3f32_multiplyScalar(v, scalar);
}
korl_internal Korl_Math_V3f32 operator/(Korl_Math_V3f32 vA, Korl_Math_V3f32 vB)
{
    return korl_math_v3f32_divide(vA, vB);
}
korl_internal Korl_Math_V3f32 operator/(Korl_Math_V3f32 v, f32 scalar)
{
    return korl_math_v3f32_divideScalar(v, scalar);
}
korl_internal Korl_Math_V3f32& operator*=(Korl_Math_V3f32& v, f32 scalar)
{
    korl_math_v3f32_assignMultiplyScalar(&v, scalar);
    return v;
}
korl_internal Korl_Math_V3f32& operator*=(Korl_Math_V3f32& vA, Korl_Math_V3f32 vB)
{
    vA = korl_math_v3f32_multiply(vA, vB);
    return vA;
}
korl_internal Korl_Math_V3f32& operator+=(Korl_Math_V3f32& vA, Korl_Math_V2f32 vB)
{
    vA = korl_math_v3f32_add(vA, korl_math_v3f32_fromV2f32Z(vB, 0));
    return vA;
}
korl_internal Korl_Math_V3f32& operator+=(Korl_Math_V3f32& vA, Korl_Math_V3f32 vB)
{
    vA = korl_math_v3f32_add(vA, vB);
    return vA;
}
korl_internal Korl_Math_V3f32& operator-=(Korl_Math_V3f32& vA, Korl_Math_V3f32 vB)
{
    vA = korl_math_v3f32_subtract(vA, vB);
    return vA;
}
korl_internal Korl_Math_V4f32  operator+(Korl_Math_V4f32 vA, Korl_Math_V4f32 vB)
{
    return korl_math_v4f32_add(vA, vB);
}
korl_internal Korl_Math_V4f32  operator+(Korl_Math_V4f32 v, f32 scalar)
{
    return korl_math_v4f32_addScalar(v, scalar);
}
korl_internal Korl_Math_V4f32  operator+(f32 scalar, Korl_Math_V4f32 v)
{
    return korl_math_v4f32_addScalar(v, scalar);
}
korl_internal Korl_Math_V4f32  operator-(Korl_Math_V4f32 vA, Korl_Math_V4f32 vB)
{
    return korl_math_v4f32_subtract(vA, vB);
}
korl_internal Korl_Math_V4f32  operator-(Korl_Math_V4f32 v, f32 scalar)
{
    return korl_math_v4f32_subtractScalar(v, scalar);
}
korl_internal Korl_Math_V4f32  operator-(f32 scalar, Korl_Math_V4f32 v)
{
    return korl_math_v4f32_subtractScalar(v, scalar);
}
korl_internal Korl_Math_V4f32  operator*(Korl_Math_V4f32 vA, Korl_Math_V4f32 vB)
{
    return korl_math_v4f32_multiply(vA, vB);
}
korl_internal Korl_Math_V4f32  operator*(Korl_Math_V4f32 v, f32 scalar)
{
    return korl_math_v4f32_multiplyScalar(v, scalar);
}
korl_internal Korl_Math_V4f32  operator*(f32 scalar, Korl_Math_V4f32 v)
{
    return korl_math_v4f32_multiplyScalar(v, scalar);
}
korl_internal Korl_Math_V4f32  operator/(Korl_Math_V4f32 vA, Korl_Math_V4f32 vB)
{
    return korl_math_v4f32_divide(vA, vB);
}
korl_internal Korl_Math_V4f32  operator/(Korl_Math_V4f32 v, f32 scalar)
{
    return korl_math_v4f32_divideScalar(v, scalar);
}
korl_internal Korl_Math_V4f32  operator/(f32 scalar, Korl_Math_V4f32 v)
{
    return korl_math_v4f32_divideScalar(v, scalar);
}
korl_internal Korl_Math_V4f32& operator+=(Korl_Math_V4f32& vA, Korl_Math_V4f32 vB)
{
    korl_math_v4f32_assignAdd(&vA, vB);
    return vA;
}
korl_internal Korl_Math_V4f32& operator+=(Korl_Math_V4f32& v, f32 scalar)
{
    korl_math_v4f32_assignAddScalar(&v, scalar);
    return v;
}
korl_internal Korl_Math_V4f32& operator-=(Korl_Math_V4f32& vA, Korl_Math_V4f32 vB)
{
    korl_math_v4f32_assignSubtract(&vA, vB);
    return vA;
}
korl_internal Korl_Math_V4f32& operator-=(Korl_Math_V4f32& v, f32 scalar)
{
    korl_math_v4f32_assignSubtractScalar(&v, scalar);
    return v;
}
korl_internal Korl_Math_V4f32& operator*=(Korl_Math_V4f32& vA, Korl_Math_V4f32 vB)
{
    korl_math_v4f32_assignMultiply(&vA, vB);
    return vA;
}
korl_internal Korl_Math_V4f32& operator*=(Korl_Math_V4f32& v, f32 scalar)
{
    korl_math_v4f32_assignMultiplyScalar(&v, scalar);
    return v;
}
korl_internal Korl_Math_V4f32& operator/=(Korl_Math_V4f32& vA, Korl_Math_V4f32 vB)
{
    korl_math_v4f32_assignDivide(&vA, vB);
    return vA;
}
korl_internal Korl_Math_V4f32& operator/=(Korl_Math_V4f32& v, f32 scalar)
{
    korl_math_v4f32_assignDivideScalar(&v, scalar);
    return v;
}
korl_internal Korl_Math_Quaternion& operator+=(Korl_Math_Quaternion& qA, Korl_Math_Quaternion qB)
{
    korl_math_v4f32_assignAdd(&qA.v4, qB.v4);
    return qA;
}
korl_internal Korl_Math_V2f32 operator*(Korl_Math_Quaternion q, Korl_Math_V2f32 v)
{
    return korl_math_quaternion_transformV2f32(q, v, false);
}
korl_internal Korl_Math_V3f32 operator*(Korl_Math_Quaternion q, Korl_Math_V3f32 v)
{
    return korl_math_quaternion_transformV3f32(q, v, false);
}
korl_internal Korl_Math_Quaternion operator*(Korl_Math_Quaternion qA, Korl_Math_Quaternion qB)
{
    return korl_math_quaternion_multiply(qA, qB);
}
korl_internal Korl_Math_V2f32& operator*=(Korl_Math_V2f32& v, Korl_Math_Quaternion q)
{
    v = korl_math_quaternion_transformV2f32(q, v, false);
    return v;
}
korl_internal Korl_Math_V3f32& operator*=(Korl_Math_V3f32& v, Korl_Math_Quaternion q)
{
    v = korl_math_quaternion_transformV3f32(q, v, false);
    return v;
}
korl_internal Korl_Math_M4f32 operator*(const Korl_Math_M4f32& mA, const Korl_Math_M4f32& mB)
{
    return korl_math_m4f32_multiply(&mA, &mB);
}
korl_internal Korl_Math_V2f32 operator*(const Korl_Math_M4f32& m, const Korl_Math_V2f32& v)
{
    Korl_Math_V4f32 v4 = korl_math_v4f32_make(v.x, v.y, 0, 1);
    return korl_math_m4f32_multiplyV4f32(&m, &v4).xy;
}
korl_internal Korl_Math_V3f32 operator*(const Korl_Math_M4f32& m, const Korl_Math_V3f32& v)
{
    return korl_math_m4f32_multiplyV3f32(&m, v);
}
korl_internal Korl_Math_Aabb2f32& operator+=(Korl_Math_Aabb2f32& aabb, Korl_Math_V2f32 v)
{
    korl_math_aabb2f32_addPointV2(&aabb, v);
    return aabb;
}
korl_internal Korl_Math_Aabb3f32& operator+=(Korl_Math_Aabb3f32& aabb, Korl_Math_V3f32 v)
{
    korl_math_aabb3f32_addPointV3(&aabb, v);
    return aabb;
}
#endif//__cplusplus
