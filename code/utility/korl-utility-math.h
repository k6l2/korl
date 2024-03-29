#pragma once
#include "korl-globalDefines.h"
#include "korl-interface-platform-memory.h"
#define KORL_PI32  3.14159f
#define KORL_TAU32 6.28318f // 2 * PI
#define KORL_MATH_CLAMP(x, min, max)        ((x) <= (min) ? (min) : ((x) >= (max) ? (max) : (x)))
#define KORL_MATH_ASSIGN_CLAMP(x, min, max) ((x) = KORL_MATH_CLAMP((x), (min), (max)))
#define KORL_MATH_ASSIGN_CLAMP_MIN(x, min)  ((x) = (x) <= (min) ? (min) : (x))
#define KORL_MATH_ASSIGN_CLAMP_MAX(x, max)  ((x) = (x) >= (max) ? (max) : (x))
#define KORL_MATH_MIN(a,b)                  ((a) <= (b) ? (a) : (b))
#define KORL_MATH_MAX(a,b)                  ((a) >= (b) ? (a) : (b))
//KORL-ISSUE-000-000-100: math: apparently, all of the math datatypes are UB in C++11+; yikes!; https://stackoverflow.com/a/31080901/4526664
typedef union Korl_Math_V2i32
{
    struct { i32 x, y; };
    i32 elements[2];
} Korl_Math_V2i32;
#define korl_math_v2i32_make(x, y) KORL_STRUCT_INITIALIZE(Korl_Math_V2i32){{x, y}}
typedef union Korl_Math_V2f32 Korl_Math_V2f32;
typedef union Korl_Math_V3f32 Korl_Math_V3f32;
typedef union Korl_Math_V3u8
{
    struct { u8 x, y, z; };
    struct { u8 r, g, b; };
    u8 elements[3];
} Korl_Math_V3u8;
typedef union Korl_Math_V4u8
{
    struct { u8 x, y, z, w; };
    struct { u8 r, g, b, a; };
    Korl_Math_V3u8 xyz;
    Korl_Math_V3u8 rgb;
    u8 elements[4];
} Korl_Math_V4u8;
#define korl_math_v4u8_make(x, y, z, w) KORL_STRUCT_INITIALIZE(Korl_Math_V4u8){{x, y, z, w}}
korl_internal inline u64  korl_math_kilobytes(u64 x);
korl_internal inline u64  korl_math_megabytes(u64 x);
korl_internal inline u64  korl_math_gigabytes(u64 x);
korl_internal inline u$   korl_math_nextHighestDivision(u$ value, u$ division);
korl_internal inline u$   korl_math_roundUpPowerOf2(u$ value, u$ powerOf2Multiple);
korl_internal inline bool korl_math_isNearlyZero(f32 x);
korl_internal inline bool korl_math_isNearlyEqualEpsilon(f32 fA, f32 fB, f32 epsilon);
korl_internal inline bool korl_math_isNearlyEqual(f32 fA, f32 fB);/** calls korl_math_isNearlyEqualEpsilon(fA, fB, 1e-5f) */
korl_internal inline f32  korl_math_f32_positive(f32 x);
korl_internal inline f32  korl_math_f32_lerp(f32 from, f32 to, f32 factor);
korl_internal inline f32  korl_math_f32_exponentialDecay(f32 from, f32 to, f32 lambdaFactor, f32 deltaTime);
/**
 * Find the roots of a given quadratic formula which satisfies the form 
 * `f(x) = ax^2 + bx + c`.  The roots are values of `x` which will yield 
 * `f(x) = 0`.  Complex# solutions are ignored.
 * \param o_roots 
 * The locations in memory where the roots will be stored, if they exist.  
 * The roots will be stored in ascending order.  Only the first N values 
 * will be written to, where N is the return value of this function.  In the 
 * RARE case that the equation is a line along the x-axis, o_roots[0] is set 
 * to INFINITY32 & a value of 0 is returned.
 * \return 
 * The number of UNIQUE real# roots found for the given equation.  This 
 * value will always be in the range [0,2].
 */
korl_internal u8          korl_math_solveQuadraticReal(f32 a, f32 b, f32 c, f32 o_roots[2]);
/* Mesh Primitive Generation **************************************************/
//KORL-PERFORMANCE-000-000-051: math: this sphere mesh generating API is crappy and does not generate vertex indices, so we will end up duplicating a ton of vertices
korl_internal u$          korl_math_generateMeshSphereVertexCount(u32 latitudeSegments, u32 longitudeSegments);
/**
 * Generate mesh data for a sphere made of (semi)circles along latitudes and 
 * longitudes with the specified respective resolutions.
 * \param latitudeSegments 
 * The # of latitude circles in the sphere equals `latitudeSegments - 1`, as 
 * the sphere's "top" and "bottom" vertices will define `latitudeSegments` 
 * strips of solid geometry.  This value MUST be >= 2.
 * \param longitudeSegments 
 * The # of longitude semicircles in the sphere.  This value MUST be >= 3.
 * \param o_vertexPositions REQUIRED;
 * Pointer to the array where the vertex data will be stored.  Caller must 
 * supply an array of >= `korl_math_generateMeshSphereVertexCount(...)` 
 * elements!
 * \param o_vertexTextureNormals OPTIONAL;
 * Vertex textureNormals are generated using a cylindrical projection.
 */
korl_internal void        korl_math_generateMeshSphere(f32 radius, u32 latitudeSegments, u32 longitudeSegments, Korl_Math_V3f32* o_vertexPositions, u$ vertexPositionByteStride, Korl_Math_V2f32* o_vertexTextureNormals, u$ vertexTextureNormalByteStride);
/* RNG ************************************************************************/
typedef struct Korl_Math_Rng_WichmannHill
{
    u16 seed[3];
} Korl_Math_Rng_WichmannHill;
korl_internal inline Korl_Math_Rng_WichmannHill korl_math_rng_wichmannHill_new(u16 seed0, u16 seed1, u16 seed2);
korl_internal inline Korl_Math_Rng_WichmannHill korl_math_rng_wichmannHill_new_u64(u64 seed);
korl_internal inline f32                        korl_math_rng_wichmannHill_f32_0_1(Korl_Math_Rng_WichmannHill*const context);
korl_internal inline f32                        korl_math_rng_wichmannHill_f32_range(Korl_Math_Rng_WichmannHill*const context, f32 range0, f32 range1);
/* V2u32 **********************************************************************/
typedef union Korl_Math_V2u32
{
    struct { u32 x, y; };
    u32 elements[2];
} Korl_Math_V2u32;
#define korl_math_v2u32_make(x, y) KORL_STRUCT_INITIALIZE(Korl_Math_V2u32){{x, y}}
#define KORL_MATH_V2U32_ZERO korl_math_v2u32_make(0, 0)
#define KORL_MATH_V2U32_ONE  korl_math_v2u32_make(1, 1)
korl_internal Korl_Math_V2u32 korl_math_v2u32_addScalar(Korl_Math_V2u32 v, u32 scalar);
korl_internal Korl_Math_V2u32 korl_math_v2u32_divide(Korl_Math_V2u32 vA, Korl_Math_V2u32 vB);
/* V2f32 **********************************************************************/
typedef union Korl_Math_V2f32
{
    struct { f32 x, y; };
    f32 elements[2];
} Korl_Math_V2f32;
#define korl_math_v2f32_make(x, y) KORL_STRUCT_INITIALIZE(Korl_Math_V2f32){{x, y}}
#define KORL_MATH_V2F32_ZERO    korl_math_v2f32_make(0,  0)
#define KORL_MATH_V2F32_X       korl_math_v2f32_make(1,  0)
#define KORL_MATH_V2F32_Y       korl_math_v2f32_make(0,  1)
#define KORL_MATH_V2F32_MINUS_X korl_math_v2f32_make(1,  0)
#define KORL_MATH_V2F32_MINUS_Y korl_math_v2f32_make(0, -1)
#define KORL_MATH_V2F32_ONE     korl_math_v2f32_make(1,  1)
/** \return a rotated V2f32 from a starting position of {radius, 0}, rotated 
 * around the +Z axis by \c radians */
korl_internal Korl_Math_V2f32 korl_math_v2f32_fromV2u32(Korl_Math_V2u32 v);
korl_internal Korl_Math_V2f32 korl_math_v2f32_fromRotationZ(f32 radius, f32 radians);
korl_internal Korl_Math_V2f32 korl_math_v2f32_rotateHalfPiZ(Korl_Math_V2f32 v);
korl_internal f32             korl_math_v2f32_magnitude(const Korl_Math_V2f32*const v);
korl_internal f32             korl_math_v2f32_magnitudeSquared(const Korl_Math_V2f32*const v);
korl_internal Korl_Math_V2f32 korl_math_v2f32_normal(Korl_Math_V2f32 v);
korl_internal Korl_Math_V2f32 korl_math_v2f32_normalKnownMagnitude(Korl_Math_V2f32 v, f32 magnitude);
korl_internal f32             korl_math_v2f32_cross(Korl_Math_V2f32 vA, Korl_Math_V2f32 vB);
korl_internal f32             korl_math_v2f32_dot(Korl_Math_V2f32 vA, Korl_Math_V2f32 vB);
korl_internal f32             korl_math_v2f32_radiansBetween(Korl_Math_V2f32 vA, Korl_Math_V2f32 vB);
korl_internal f32             korl_math_v2f32_radiansZ(Korl_Math_V2f32 v);
korl_internal Korl_Math_V2f32 korl_math_v2f32_project(Korl_Math_V2f32 vA, Korl_Math_V2f32 vB, bool vbIsNormalized);
korl_internal Korl_Math_V2f32 korl_math_v2f32_add(Korl_Math_V2f32 vA, Korl_Math_V2f32 vB);
// korl_internal Korl_Math_V2f32 korl_math_v2f32_addScalar(Korl_Math_V2f32 v, f32 scalar);
korl_internal Korl_Math_V2f32 korl_math_v2f32_subtract(Korl_Math_V2f32 vA, Korl_Math_V2f32 vB);
// korl_internal Korl_Math_V2f32 korl_math_v2f32_subtractScalar(Korl_Math_V2f32 v, f32 scalar);
korl_internal Korl_Math_V2f32 korl_math_v2f32_multiply(Korl_Math_V2f32 vA, Korl_Math_V2f32 vB);
korl_internal Korl_Math_V2f32 korl_math_v2f32_multiplyScalar(Korl_Math_V2f32 v, f32 scalar);
korl_internal Korl_Math_V2f32 korl_math_v2f32_divide(Korl_Math_V2f32 vA, Korl_Math_V2f32 vB);
korl_internal Korl_Math_V2f32 korl_math_v2f32_divideScalar(Korl_Math_V2f32 v, f32 scalar);
korl_internal void            korl_math_v2f32_assignAdd(Korl_Math_V2f32*const vA, Korl_Math_V2f32 vB);
korl_internal void            korl_math_v2f32_assignAddScalar(Korl_Math_V2f32*const v, f32 scalar);
korl_internal void            korl_math_v2f32_assignSubtract(Korl_Math_V2f32*const vA, Korl_Math_V2f32 vB);
korl_internal void            korl_math_v2f32_assignSubtractScalar(Korl_Math_V2f32*const v, f32 scalar);
korl_internal void            korl_math_v2f32_assignMultiply(Korl_Math_V2f32*const vA, Korl_Math_V2f32 vB);
korl_internal void            korl_math_v2f32_assignMultiplyScalar(Korl_Math_V2f32*const v, f32 scalar);
korl_internal Korl_Math_V2f32 korl_math_v2f32_min(Korl_Math_V2f32 vA, Korl_Math_V2f32 vB);
korl_internal Korl_Math_V2f32 korl_math_v2f32_max(Korl_Math_V2f32 vA, Korl_Math_V2f32 vB);
korl_internal Korl_Math_V2f32 korl_math_v2f32_nan(void);
korl_internal bool            korl_math_v2f32_hasNan(Korl_Math_V2f32 v);
korl_internal Korl_Math_V2f32 korl_math_v2f32_positive(Korl_Math_V2f32 v);
korl_internal bool            korl_math_v2f32_isNearlyZero(Korl_Math_V2f32 v);
/* V3f32 **********************************************************************/
typedef union Korl_Math_V3f32
{
    struct { f32 x, y, z; };
    struct {Korl_Math_V2f32 xy; f32 _z;};
    f32 elements[3];
} Korl_Math_V3f32;
#define korl_math_v3f32_make(x, y, z) KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){{x, y, z}}
#define korl_math_v3f32_make2d(x, y)  KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){{x, y, 0}}
#define KORL_MATH_V3F32_ONE     korl_math_v3f32_make(1,  1,  1)
#define KORL_MATH_V3F32_ZERO    korl_math_v3f32_make(0,  0,  0)
#define KORL_MATH_V3F32_X       korl_math_v3f32_make(1,  0,  0)
#define KORL_MATH_V3F32_Y       korl_math_v3f32_make(0,  1,  0)
#define KORL_MATH_V3F32_Z       korl_math_v3f32_make(0,  0,  1)
#define KORL_MATH_V3F32_MINUS_X korl_math_v3f32_make(1,  0,  0)
#define KORL_MATH_V3F32_MINUS_Y korl_math_v3f32_make(0, -1,  0)
#define KORL_MATH_V3F32_MINUS_Z korl_math_v3f32_make(0,  0, -1)
korl_internal Korl_Math_V3f32 korl_math_v3f32_fromV2f32Z(Korl_Math_V2f32 v2, f32 z);
korl_internal f32             korl_math_v3f32_magnitude(const Korl_Math_V3f32*const v);
korl_internal f32             korl_math_v3f32_magnitudeSquared(const Korl_Math_V3f32*const v);
korl_internal Korl_Math_V3f32 korl_math_v3f32_normal(Korl_Math_V3f32 v);
korl_internal Korl_Math_V3f32 korl_math_v3f32_normalKnownMagnitude(Korl_Math_V3f32 v, f32 magnitude);
korl_internal f32             korl_math_v3f32_normalize(Korl_Math_V3f32* v);
korl_internal Korl_Math_V3f32 korl_math_v3f32_cross(Korl_Math_V3f32 vA, Korl_Math_V3f32 vB);
korl_internal Korl_Math_V3f32 korl_math_v3f32_tripleProduct(Korl_Math_V3f32 vA, Korl_Math_V3f32 vB, Korl_Math_V3f32 vC);// (vA x vB) x vC
korl_internal f32             korl_math_v3f32_dot(Korl_Math_V3f32 vA, Korl_Math_V3f32 vB);
korl_internal f32             korl_math_v3f32_radiansBetween(Korl_Math_V3f32 vA, Korl_Math_V3f32 vB);
korl_internal Korl_Math_V3f32 korl_math_v3f32_project(Korl_Math_V3f32 vA, Korl_Math_V3f32 vB, bool vbIsNormalized);
korl_internal Korl_Math_V3f32 korl_math_v3f32_add(Korl_Math_V3f32 vA, Korl_Math_V3f32 vB);
// korl_internal Korl_Math_V3f32 korl_math_v3f32_addScalar(Korl_Math_V3f32 v, f32 scalar);
korl_internal Korl_Math_V3f32 korl_math_v3f32_subtract(Korl_Math_V3f32 vA, Korl_Math_V3f32 vB);
// korl_internal Korl_Math_V3f32 korl_math_v3f32_subtractScalar(Korl_Math_V3f32 v, f32 scalar);
korl_internal Korl_Math_V3f32 korl_math_v3f32_multiply(Korl_Math_V3f32 vA, Korl_Math_V3f32 vB);
korl_internal Korl_Math_V3f32 korl_math_v3f32_multiplyScalar(Korl_Math_V3f32 v, f32 scalar);
korl_internal Korl_Math_V3f32 korl_math_v3f32_divide(Korl_Math_V3f32 vA, Korl_Math_V3f32 vB);
korl_internal Korl_Math_V3f32 korl_math_v3f32_divideScalar(Korl_Math_V3f32 v, f32 scalar);
korl_internal void            korl_math_v3f32_assignAdd(Korl_Math_V3f32*const vA, Korl_Math_V3f32 vB);
korl_internal void            korl_math_v3f32_assignAddScalar(Korl_Math_V3f32*const v, f32 scalar);
korl_internal void            korl_math_v3f32_assignSubtract(Korl_Math_V3f32*const vA, Korl_Math_V3f32 vB);
korl_internal void            korl_math_v3f32_assignSubtractScalar(Korl_Math_V3f32*const v, f32 scalar);
korl_internal void            korl_math_v3f32_assignMultiply(Korl_Math_V3f32*const vA, Korl_Math_V3f32 vB);
korl_internal void            korl_math_v3f32_assignMultiplyScalar(Korl_Math_V3f32*const v, f32 scalar);
korl_internal Korl_Math_V3f32 korl_math_v3f32_interpolateLinear(Korl_Math_V3f32 vFactorZero, Korl_Math_V3f32 vFactorOne, f32 factor);
// korl_internal Korl_Math_V3f32 korl_math_v3f32_min(Korl_Math_V3f32 vA, Korl_Math_V3f32 vB);
// korl_internal Korl_Math_V3f32 korl_math_v3f32_max(Korl_Math_V3f32 vA, Korl_Math_V3f32 vB);
korl_internal bool            korl_math_v3f32_isNearlyZero(Korl_Math_V3f32 v);
/* V4u8 ***********************************************************************/
korl_internal Korl_Math_V4u8 korl_math_v4u8_lerp(Korl_Math_V4u8 from, Korl_Math_V4u8 to, f32 factor);
/* V4f32 **********************************************************************/
typedef union Korl_Math_V4f32
{
    struct { f32 x, y, z, w; };
    struct { Korl_Math_V2f32 xy, zw; };
    struct { Korl_Math_V3f32 xyz; f32 _w; };
    f32 elements[4];
} Korl_Math_V4f32;
#define korl_math_v4f32_make(x, y, z, w) KORL_STRUCT_INITIALIZE(Korl_Math_V4f32){{x, y, z, w}}
#define KORL_MATH_V4F32_ONE  korl_math_v4f32_make(1, 1, 1, 1)
#define KORL_MATH_V4F32_ZERO korl_math_v4f32_make(0, 0, 0, 0)
korl_internal f32              korl_math_v4f32_magnitude(const Korl_Math_V4f32*const v);
korl_internal f32              korl_math_v4f32_magnitudeSquared(const Korl_Math_V4f32*const v);
korl_internal Korl_Math_V4f32  korl_math_v4f32_normal(Korl_Math_V4f32 v);
korl_internal Korl_Math_V4f32  korl_math_v4f32_normalKnownMagnitude(Korl_Math_V4f32 v, f32 magnitude);
korl_internal f32              korl_math_v4f32_dot(const Korl_Math_V4f32*const vA, const Korl_Math_V4f32*const vB);
korl_internal Korl_Math_V4f32  korl_math_v4f32_add(Korl_Math_V4f32 vA, Korl_Math_V4f32 vB);
korl_internal Korl_Math_V4f32  korl_math_v4f32_addScalar(Korl_Math_V4f32 v, f32 scalar);
korl_internal Korl_Math_V4f32  korl_math_v4f32_subtract(Korl_Math_V4f32 vA, Korl_Math_V4f32 vB);
korl_internal Korl_Math_V4f32  korl_math_v4f32_subtractScalar(Korl_Math_V4f32 v, f32 scalar);
korl_internal Korl_Math_V4f32  korl_math_v4f32_multiply(Korl_Math_V4f32 vA, Korl_Math_V4f32 vB);
korl_internal Korl_Math_V4f32  korl_math_v4f32_multiplyScalar(Korl_Math_V4f32 v, f32 scalar);
korl_internal Korl_Math_V4f32  korl_math_v4f32_divide(Korl_Math_V4f32 vA, Korl_Math_V4f32 vB);
korl_internal Korl_Math_V4f32  korl_math_v4f32_divideScalar(Korl_Math_V4f32 v, f32 scalar);
korl_internal Korl_Math_V4f32* korl_math_v4f32_assignAdd(Korl_Math_V4f32*const vA, Korl_Math_V4f32 vB);
korl_internal Korl_Math_V4f32* korl_math_v4f32_assignAddScalar(Korl_Math_V4f32*const v, f32 scalar);
korl_internal Korl_Math_V4f32* korl_math_v4f32_assignSubtract(Korl_Math_V4f32*const vA, Korl_Math_V4f32 vB);
korl_internal Korl_Math_V4f32* korl_math_v4f32_assignSubtractScalar(Korl_Math_V4f32*const v, f32 scalar);
korl_internal Korl_Math_V4f32* korl_math_v4f32_assignMultiply(Korl_Math_V4f32*const vA, Korl_Math_V4f32 vB);
korl_internal Korl_Math_V4f32* korl_math_v4f32_assignMultiplyScalar(Korl_Math_V4f32*const v, f32 scalar);
korl_internal Korl_Math_V4f32* korl_math_v4f32_assignDivide(Korl_Math_V4f32*const vA, Korl_Math_V4f32 vB);
korl_internal Korl_Math_V4f32* korl_math_v4f32_assignDivideScalar(Korl_Math_V4f32*const v, f32 scalar);
korl_internal Korl_Math_V4f32  korl_math_v4f32_interpolateLinear(Korl_Math_V4f32 vFactorZero, Korl_Math_V4f32 vFactorOne, f32 factor);
// korl_internal Korl_Math_V4f32 korl_math_v4f32_min(Korl_Math_V4f32 vA, Korl_Math_V4f32 vB);
// korl_internal Korl_Math_V4f32 korl_math_v4f32_max(Korl_Math_V4f32 vA, Korl_Math_V4f32 vB);
/* Quaternion *****************************************************************/
typedef union Korl_Math_Quaternion
{
    struct { f32 x, y, z, w; };
    struct { Korl_Math_V3f32 xyz; f32 _w; };
    Korl_Math_V4f32 v4;// we do this instead of just `typedef V4f32 Quaternion` because in C++ we _must_ differentiate `quaternion operator*` with `v4f32 operator*`
    f32 elements[4];
} Korl_Math_Quaternion;
#define korl_math_quaternion_make(x, y, z, w) KORL_STRUCT_INITIALIZE(Korl_Math_Quaternion){{x, y, z, w}}
#define KORL_MATH_QUATERNION_IDENTITY korl_math_quaternion_make(0, 0, 0, 1)
korl_internal Korl_Math_Quaternion korl_math_quaternion_normal(Korl_Math_Quaternion q);
korl_internal f32                  korl_math_quaternion_normalize(Korl_Math_Quaternion* q);/** \return the magnitude of \c q */
korl_internal Korl_Math_Quaternion korl_math_quaternion_fromAxisRadians(Korl_Math_V3f32 axis, f32 radians, bool axisIsNormalized);
korl_internal Korl_Math_Quaternion korl_math_quaternion_fromVector(Korl_Math_V3f32 forward, Korl_Math_V3f32 worldForward, Korl_Math_V3f32 worldUp);
korl_internal Korl_Math_Quaternion korl_math_quaternion_multiply(Korl_Math_Quaternion qA, Korl_Math_Quaternion qB);
korl_internal Korl_Math_Quaternion korl_math_quaternion_hamilton(Korl_Math_Quaternion qA, Korl_Math_Quaternion qB);
korl_internal Korl_Math_Quaternion korl_math_quaternion_conjugate(Korl_Math_Quaternion q);
korl_internal Korl_Math_V2f32      korl_math_quaternion_transformV2f32(Korl_Math_Quaternion q, Korl_Math_V2f32 v, bool qIsNormalized);
korl_internal Korl_Math_V3f32      korl_math_quaternion_transformV3f32(Korl_Math_Quaternion q, Korl_Math_V3f32 v, bool qIsNormalized);
/* M4f32 **********************************************************************/
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
    };
} Korl_Math_M4f32;
#define korl_math_m4f32_make(v4f32Row0, v4f32Row1, v4f32Row2, v4f32Row3) KORL_STRUCT_INITIALIZE(Korl_Math_M4f32){{v4f32Row0, v4f32Row1, v4f32Row2, v4f32Row3}}
#define KORL_MATH_M4F32_IDENTITY korl_math_m4f32_make(korl_math_v4f32_make(1, 0, 0, 0)\
                                                     ,korl_math_v4f32_make(0, 1, 0, 0)\
                                                     ,korl_math_v4f32_make(0, 0, 1, 0)\
                                                     ,korl_math_v4f32_make(0, 0, 0, 1))
korl_internal Korl_Math_M4f32 korl_math_makeM4f32_rotate(Korl_Math_Quaternion qRotation);
korl_internal Korl_Math_M4f32 korl_math_makeM4f32_rotateTranslate(Korl_Math_Quaternion qRotation, Korl_Math_V3f32 vTranslation);
korl_internal Korl_Math_M4f32 korl_math_makeM4f32_rotateScaleTranslate(Korl_Math_Quaternion qRotation, Korl_Math_V3f32 vScale, Korl_Math_V3f32 vTranslation);
korl_internal void            korl_math_m4f32_decompose(Korl_Math_M4f32 m, Korl_Math_V3f32* o_translation, Korl_Math_Quaternion* o_rotation, Korl_Math_V3f32* o_scale);
korl_internal Korl_Math_M4f32 korl_math_m4f32_transpose(const Korl_Math_M4f32*const m);
/** If matrix \c m cannot be inverted, then the [0][0] element of the resulting 
 * matrix will be set to \c NaN , and all other elements of the result are \c undefined .
 */
korl_internal Korl_Math_M4f32 korl_math_m4f32_invert(const Korl_Math_M4f32*const m);
korl_internal Korl_Math_M4f32 korl_math_m4f32_multiply(const Korl_Math_M4f32*const mA, const Korl_Math_M4f32*const mB);
korl_internal Korl_Math_V3f32 korl_math_m4f32_multiplyV3f32(const Korl_Math_M4f32*const m, Korl_Math_V3f32 v);
korl_internal Korl_Math_V4f32 korl_math_m4f32_multiplyV4f32(const Korl_Math_M4f32*const m, const Korl_Math_V4f32*const v);
korl_internal Korl_Math_V4f32 korl_math_m4f32_multiplyV4f32Copy(const Korl_Math_M4f32*const m, Korl_Math_V4f32 v);
korl_internal Korl_Math_M4f32 korl_math_m4f32_projectionFov(f32 verticalFovDegrees, f32 viewportWidthOverHeight, f32 clipNear, f32 clipFar);
korl_internal Korl_Math_M4f32 korl_math_m4f32_projectionOrthographic(f32 xMin, f32 xMax, f32 yMin, f32 yMax, f32 zMin, f32 zMax);
korl_internal Korl_Math_M4f32 korl_math_m4f32_lookAt(const Korl_Math_V3f32*const positionEye, const Korl_Math_V3f32*const positionTarget, const Korl_Math_V3f32*const worldUpNormal);
korl_internal bool korl_math_m4f32_isNearEqual(const Korl_Math_M4f32*const mA, const Korl_Math_M4f32*const mB);
/* Aabb2f32 *******************************************************************/
typedef struct Korl_Math_Aabb2f32
{
    Korl_Math_V2f32 min;
    Korl_Math_V2f32 max;
} Korl_Math_Aabb2f32;
#define korl_math_aabb2f32_make(v2f32Min, v2f32Max) KORL_STRUCT_INITIALIZE(Korl_Math_Aabb2f32){v2f32Min, v2f32Max}
#define KORL_MATH_AABB2F32_EMPTY korl_math_aabb2f32_make(korl_math_v2f32_make( KORL_F32_MAX, KORL_F32_MAX)\
                                                        ,korl_math_v2f32_make(-KORL_F32_MAX,-KORL_F32_MAX))
korl_internal Korl_Math_Aabb2f32 korl_math_aabb2f32_fromPoints(f32 p0x, f32 p0y, f32 p1x, f32 p1y);
korl_internal Korl_Math_Aabb2f32 korl_math_aabb2f32_fromPointsV2(const Korl_Math_V2f32* points, u$ pointsSize);
korl_internal Korl_Math_Aabb2f32 korl_math_aabb2f32_fromExpanded(Korl_Math_Aabb2f32 aabb, f32 expandX, f32 expandY);
korl_internal Korl_Math_Aabb2f32 korl_math_aabb2f32_fromExpandedV2(Korl_Math_V2f32 v, f32 expandX, f32 expandY);
korl_internal void               korl_math_aabb2f32_expand(Korl_Math_Aabb2f32*const aabb, f32 expandXY);
korl_internal Korl_Math_V2f32    korl_math_aabb2f32_size(Korl_Math_Aabb2f32 aabb);
korl_internal bool               korl_math_aabb2f32_contains(Korl_Math_Aabb2f32 aabb, f32 x, f32 y);
korl_internal bool               korl_math_aabb2f32_containsV2f32(Korl_Math_Aabb2f32 aabb, Korl_Math_V2f32 v);
korl_internal Korl_Math_Aabb2f32 korl_math_aabb2f32_union(Korl_Math_Aabb2f32 aabbA, Korl_Math_Aabb2f32 aabbB);
korl_internal Korl_Math_Aabb2f32 korl_math_aabb2f32_intersect(Korl_Math_Aabb2f32 aabbA, Korl_Math_Aabb2f32 aabbB);
korl_internal bool               korl_math_aabb2f32_areIntersecting(Korl_Math_Aabb2f32 aabbA, Korl_Math_Aabb2f32 aabbB);
korl_internal void               korl_math_aabb2f32_addPoint(Korl_Math_Aabb2f32*const aabb, f32* point2d);
korl_internal void               korl_math_aabb2f32_addPointV2(Korl_Math_Aabb2f32*const aabb, Korl_Math_V2f32 point);
/* Aabb3f32 *******************************************************************/
typedef struct Korl_Math_Aabb3f32
{
    Korl_Math_V3f32 min;
    Korl_Math_V3f32 max;
} Korl_Math_Aabb3f32;
#define korl_math_aabb3f32_make(v3f32Min, v3f32Max) KORL_STRUCT_INITIALIZE(Korl_Math_Aabb3f32){v3f32Min, v3f32Max}
#define KORL_MATH_AABB3F32_EMPTY korl_math_aabb3f32_make(korl_math_v3f32_make( KORL_F32_MAX, KORL_F32_MAX, KORL_F32_MAX)\
                                                        ,korl_math_v3f32_make(-KORL_F32_MAX,-KORL_F32_MAX,-KORL_F32_MAX))
korl_internal Korl_Math_Aabb3f32 korl_math_aabb3f32_fromPoints(f32 p0x, f32 p0y, f32 p0z, f32 p1x, f32 p1y, f32 p1z);
// korl_internal Korl_Math_Aabb3f32 korl_math_aabb3f32_fromExpandedV3(Korl_Math_Aabb3f32 aabb, f32 expandX, f32 expandY, f32 expandZ);
korl_internal Korl_Math_Aabb3f32 korl_math_aabb3f32_fromExpandedV3(Korl_Math_V3f32 v, Korl_Math_V3f32 vExpansion);
korl_internal void               korl_math_aabb3f32_expand(Korl_Math_Aabb3f32*const aabb, f32 expandXYZ);
korl_internal Korl_Math_V3f32    korl_math_aabb3f32_size(Korl_Math_Aabb3f32 aabb);
korl_internal bool               korl_math_aabb3f32_contains(Korl_Math_Aabb3f32 aabb, f32 x, f32 y, f32 z);
korl_internal bool               korl_math_aabb3f32_containsV3f32(Korl_Math_Aabb3f32 aabb, Korl_Math_V3f32 v);
korl_internal Korl_Math_Aabb3f32 korl_math_aabb3f32_union(Korl_Math_Aabb3f32 aabbA, Korl_Math_Aabb3f32 aabbB);
korl_internal bool               korl_math_aabb3f32_areIntersecting(Korl_Math_Aabb3f32 aabbA, Korl_Math_Aabb3f32 aabbB);
korl_internal void               korl_math_aabb3f32_addPoint(Korl_Math_Aabb3f32*const aabb, f32* point3d);
korl_internal void               korl_math_aabb3f32_addPointV3(Korl_Math_Aabb3f32*const aabb, Korl_Math_V3f32 point);
/* Korl_Math_Transform3d ******************************************************/
/** Users of this struct are _highly_ advised to use the associated APIs to 
 * modify/access \c Korl_Math_Transform3d data, as one of the primary 
 * functionalities is lazy transform matrix calculations. */
typedef struct Korl_Math_Transform3d
{
    Korl_Math_M4f32      _m4f32;
    Korl_Math_Quaternion _versor;
    Korl_Math_V3f32      _position;
    Korl_Math_V3f32      _scale;
    bool                 _m4f32IsUpdated;
} Korl_Math_Transform3d;
korl_internal Korl_Math_Transform3d korl_math_transform3d_identity(void);
korl_internal Korl_Math_Transform3d korl_math_transform3d_rotateTranslate(Korl_Math_Quaternion versor, Korl_Math_V3f32 position);
korl_internal Korl_Math_Transform3d korl_math_transform3d_rotateScaleTranslate(Korl_Math_Quaternion versor, Korl_Math_V3f32 scale, Korl_Math_V3f32 position);
korl_internal void                  korl_math_transform3d_setPosition(Korl_Math_Transform3d* context, Korl_Math_V3f32 position);
korl_internal void                  korl_math_transform3d_move(Korl_Math_Transform3d* context, Korl_Math_V3f32 positionDelta);
korl_internal void                  korl_math_transform3d_setVersor(Korl_Math_Transform3d* context, Korl_Math_Quaternion versor);
korl_internal void                  korl_math_transform3d_setScale(Korl_Math_Transform3d* context, Korl_Math_V3f32 scale);
korl_internal void                  korl_math_transform3d_setM4f32(Korl_Math_Transform3d* context, Korl_Math_M4f32 m4f32);
korl_internal void                  korl_math_transform3d_updateM4f32(Korl_Math_Transform3d* context);
korl_internal Korl_Math_M4f32       korl_math_transform3d_m4f32(Korl_Math_Transform3d* context);
/* Korl_Math_TriangleMesh *****************************************************/
typedef struct Korl_Math_TriangleMesh
{
    Korl_Memory_AllocatorHandle allocator;
    Korl_Math_V3f32*            stbDaVertices;
    u32*                        stbDaIndices;
    Korl_Math_Aabb3f32          aabb;
} Korl_Math_TriangleMesh;
korl_internal Korl_Math_TriangleMesh korl_math_triangleMesh_create(Korl_Memory_AllocatorHandle allocator);
korl_internal void                   korl_math_triangleMesh_destroy(Korl_Math_TriangleMesh* context);
korl_internal void                   korl_math_triangleMesh_collectDefragmentPointers(Korl_Math_TriangleMesh* context, void* stbDaMemoryContext, Korl_Heap_DefragmentPointer** pStbDaDefragmentPointers, void* parent);
korl_internal void                   korl_math_triangleMesh_add(Korl_Math_TriangleMesh* context, const Korl_Math_V3f32* vertices, u32 verticesSize);
korl_internal void                   korl_math_triangleMesh_addIndexed(Korl_Math_TriangleMesh* context, const Korl_Math_V3f32* vertices, u32 verticesSize, const u32* indices, u32 indicesSize);
/* Collision Test Primitives **************************************************/
// Good resource on intersection test algorithms: https://www.realtimerendering.com/intersections.html
/**
 * \return 
 * (1) NAN32 if the ray does not collide with the plane.  
 * (2) INFINITY32 if the ray is co-planar with the plane.  
 * (3) Otherwise, a scalar representing how far from `rayOrigin` in the 
 *     direction of `rayNormal` the intersection is.
 * In case (3), the return value will ALWAYS be positive.
 */
korl_internal f32 korl_math_collide_ray_plane(const Korl_Math_V3f32 rayOrigin, const Korl_Math_V3f32 rayNormal
                                             ,const Korl_Math_V3f32 planeNormal, f32 planeDistanceFromOrigin
                                             ,bool cullPlaneBackFace);
/**
 * \return 
 * (1) NAN32 if the ray does not collide with the sphere
 * (2) Otherwise, a scalar representing how far from `rayOrigin` in 
 *     the direction of `rayNormal` the intersection is.
 * If the ray origin is contained within the sphere, 0 is returned. 
 */
korl_internal f32 korl_math_collide_ray_sphere(const Korl_Math_V3f32 rayOrigin, const Korl_Math_V3f32 rayNormal
                                              ,const Korl_Math_V3f32 sphereOrigin, f32 sphereRadius);
/* C++ API ********************************************************************/
#ifdef __cplusplus
korl_internal Korl_Math_V2u32 operator+(Korl_Math_V2u32 v, u32 scalar);
korl_internal Korl_Math_V2u32 operator/(Korl_Math_V2u32 vNumerator, Korl_Math_V2u32 vDenominator);
korl_internal Korl_Math_V2u32& operator/=(Korl_Math_V2u32& vA, Korl_Math_V2u32 vB);
korl_internal Korl_Math_V2f32 operator+(Korl_Math_V2f32 vA, Korl_Math_V2f32 vB);
// korl_internal Korl_Math_V2f32 operator+(Korl_Math_V2f32 v, f32 scalar);
// korl_internal Korl_Math_V2f32 operator+(f32 scalar, Korl_Math_V2f32 v);
korl_internal Korl_Math_V2f32 operator-(Korl_Math_V2f32 v);
korl_internal Korl_Math_V2f32 operator-(Korl_Math_V2f32 vA, Korl_Math_V2f32 vB);
// korl_internal Korl_Math_V2f32 operator-(Korl_Math_V2f32 v, f32 scalar);
// korl_internal Korl_Math_V2f32 operator-(f32 scalar, Korl_Math_V2f32 v);
korl_internal Korl_Math_V2f32 operator*(Korl_Math_V2f32 vA, Korl_Math_V2f32 vB);// component-wise multiplication
korl_internal Korl_Math_V2f32 operator*(Korl_Math_V2f32 v, f32 scalar);
korl_internal Korl_Math_V2f32 operator*(f32 scalar, Korl_Math_V2f32 v);
korl_internal Korl_Math_V2f32& operator*=(Korl_Math_V2f32& v, f32 scalar);
// korl_internal Korl_Math_V2f32 operator/(Korl_Math_V2f32 vA, Korl_Math_V2f32 vB);
korl_internal Korl_Math_V2f32 operator/(Korl_Math_V2f32 v, f32 scalar);
korl_internal Korl_Math_V2f32& operator/=(Korl_Math_V2f32& v, f32 scalar);
// korl_internal Korl_Math_V2f32 operator/(f32 scalar, Korl_Math_V2f32 v);
korl_internal Korl_Math_V2f32& operator+=(Korl_Math_V2f32& vA, Korl_Math_V2f32 vB);
korl_internal Korl_Math_V2f32& operator-=(Korl_Math_V2f32& vA, Korl_Math_V2f32 vB);
korl_internal Korl_Math_V2f32& operator*=(Korl_Math_V2f32& vA, Korl_Math_V2f32 vB);
korl_internal Korl_Math_V3f32 operator+(Korl_Math_V3f32 vA, Korl_Math_V3f32 vB);
// korl_internal Korl_Math_V3f32 operator+(Korl_Math_V3f32 v, f32 scalar);
// korl_internal Korl_Math_V3f32 operator+(f32 scalar, Korl_Math_V3f32 v);
korl_internal Korl_Math_V3f32 operator-(Korl_Math_V3f32 v);
korl_internal Korl_Math_V3f32 operator-(Korl_Math_V3f32 vA, Korl_Math_V3f32 vB);
// korl_internal Korl_Math_V3f32 operator-(Korl_Math_V3f32 v, f32 scalar);
// korl_internal Korl_Math_V3f32 operator-(f32 scalar, Korl_Math_V3f32 v);
// korl_internal Korl_Math_V3f32 operator*(Korl_Math_V3f32 vA, Korl_Math_V3f32 vB);// component-wise multiplication
korl_internal Korl_Math_V3f32 operator*(Korl_Math_V3f32 v, f32 scalar);
korl_internal Korl_Math_V3f32 operator*(f32 scalar, Korl_Math_V3f32 v);
korl_internal Korl_Math_V3f32 operator/(Korl_Math_V3f32 vA, Korl_Math_V3f32 vB);
korl_internal Korl_Math_V3f32 operator/(Korl_Math_V3f32 v, f32 scalar);
// korl_internal Korl_Math_V3f32 operator/(f32 scalar, Korl_Math_V3f32 v);
korl_internal Korl_Math_V3f32& operator*=(Korl_Math_V3f32& v, f32 scalar);
korl_internal Korl_Math_V3f32& operator*=(Korl_Math_V3f32& vA, Korl_Math_V3f32 vB);
korl_internal Korl_Math_V3f32& operator+=(Korl_Math_V3f32& vA, Korl_Math_V2f32 vB);
korl_internal Korl_Math_V3f32& operator+=(Korl_Math_V3f32& vA, Korl_Math_V3f32 vB);
korl_internal Korl_Math_V3f32& operator-=(Korl_Math_V3f32& vA, Korl_Math_V3f32 vB);
korl_internal Korl_Math_V4f32  operator+(Korl_Math_V4f32 vA, Korl_Math_V4f32 vB);
korl_internal Korl_Math_V4f32  operator+(Korl_Math_V4f32 v, f32 scalar);
korl_internal Korl_Math_V4f32  operator+(f32 scalar, Korl_Math_V4f32 v);
korl_internal Korl_Math_V4f32  operator-(Korl_Math_V4f32 vA, Korl_Math_V4f32 vB);
korl_internal Korl_Math_V4f32  operator-(Korl_Math_V4f32 v, f32 scalar);
korl_internal Korl_Math_V4f32  operator-(f32 scalar, Korl_Math_V4f32 v);
korl_internal Korl_Math_V4f32  operator*(Korl_Math_V4f32 vA, Korl_Math_V4f32 vB);// component-wise multiplication
korl_internal Korl_Math_V4f32  operator*(Korl_Math_V4f32 v, f32 scalar);
korl_internal Korl_Math_V4f32  operator*(f32 scalar, Korl_Math_V4f32 v);
korl_internal Korl_Math_V4f32  operator/(Korl_Math_V4f32 vA, Korl_Math_V4f32 vB);
korl_internal Korl_Math_V4f32  operator/(Korl_Math_V4f32 v, f32 scalar);
korl_internal Korl_Math_V4f32  operator/(f32 scalar, Korl_Math_V4f32 v);
korl_internal Korl_Math_V4f32& operator+=(Korl_Math_V4f32& vA, Korl_Math_V4f32 vB);
korl_internal Korl_Math_V4f32& operator+=(Korl_Math_V4f32& v, f32 scalar);
korl_internal Korl_Math_V4f32& operator-=(Korl_Math_V4f32& vA, Korl_Math_V4f32 vB);
korl_internal Korl_Math_V4f32& operator-=(Korl_Math_V4f32& v, f32 scalar);
korl_internal Korl_Math_V4f32& operator*=(Korl_Math_V4f32& vA, Korl_Math_V4f32 vB);
korl_internal Korl_Math_V4f32& operator*=(Korl_Math_V4f32& v, f32 scalar);
korl_internal Korl_Math_V4f32& operator/=(Korl_Math_V4f32& vA, Korl_Math_V4f32 vB);
korl_internal Korl_Math_V4f32& operator/=(Korl_Math_V4f32& v, f32 scalar);
korl_internal Korl_Math_Quaternion& operator+=(Korl_Math_Quaternion& qA, Korl_Math_Quaternion qB);
korl_internal Korl_Math_V2f32 operator*(Korl_Math_Quaternion q, Korl_Math_V2f32 v);
korl_internal Korl_Math_V3f32 operator*(Korl_Math_Quaternion q, Korl_Math_V3f32 v);
korl_internal Korl_Math_V2f32& operator*=(Korl_Math_V2f32& v, Korl_Math_Quaternion q);
korl_internal Korl_Math_V3f32& operator*=(Korl_Math_V3f32& v, Korl_Math_Quaternion q);
korl_internal Korl_Math_Quaternion operator*(Korl_Math_Quaternion qA, Korl_Math_Quaternion qB);
korl_internal Korl_Math_M4f32 operator*(const Korl_Math_M4f32& mA, const Korl_Math_M4f32& mB);
korl_internal Korl_Math_V2f32 operator*(const Korl_Math_M4f32& m, const Korl_Math_V2f32& v);
korl_internal Korl_Math_V3f32 operator*(const Korl_Math_M4f32& m, const Korl_Math_V3f32& v);
korl_internal Korl_Math_Aabb2f32& operator+=(Korl_Math_Aabb2f32& aabb, Korl_Math_V2f32 v);
korl_internal Korl_Math_Aabb3f32& operator+=(Korl_Math_Aabb3f32& aabb, Korl_Math_V3f32 v);
#endif//__cplusplus
