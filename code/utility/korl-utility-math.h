#pragma once
#include "korl-globalDefines.h"
#define KORL_PI32  3.14159f
#define KORL_TAU32 6.28318f // 2*PI
#define KORL_MATH_CLAMP(x, min, max)        ((x) <= (min) ? (min) : ((x) >= (max) ? (max) : (x)))
#define KORL_MATH_ASSIGN_CLAMP(x, min, max) ((x) = KORL_MATH_CLAMP((x), (min), (max)))
#define KORL_MATH_ASSIGN_CLAMP_MIN(x, min)  ((x) = (x) <= (min) ? (min) : (x))
#define KORL_MATH_ASSIGN_CLAMP_MAX(x, max)  ((x) = (x) >= (max) ? (max) : (x))
#define KORL_MATH_MIN(a,b)                  ((a) <= (b) ? (a) : (b))
#define KORL_MATH_MAX(a,b)                  ((a) >= (b) ? (a) : (b))
//KORL-ISSUE-000-000-100: math: apparently, all of the math datatypes are UB in C++11+; yikes!; https://stackoverflow.com/a/31080901/4526664
typedef union Korl_Math_V2u32
{
    struct { u32 x, y; };
    u32 elements[2];
} Korl_Math_V2u32;
const Korl_Math_V2u32 KORL_MATH_V2U32_ZERO = {0, 0};
const Korl_Math_V2u32 KORL_MATH_V2U32_ONE  = {1, 1};
typedef union Korl_Math_V2i32
{
    struct { i32 x, y; };
    i32 elements[2];
} Korl_Math_V2i32;
typedef union Korl_Math_V2f32
{
    struct { f32 x, y; };
    f32 elements[2];
} Korl_Math_V2f32;
const Korl_Math_V2f32 KORL_MATH_V2F32_ZERO = {0, 0};
const Korl_Math_V2f32 KORL_MATH_V2F32_X    = {1, 0};
const Korl_Math_V2f32 KORL_MATH_V2F32_Y    = {0, 1};
const Korl_Math_V2f32 KORL_MATH_V2F32_ONE  = {1, 1};
typedef union Korl_Math_V3f32
{
    struct { f32 x, y, z; };
    struct {Korl_Math_V2f32 xy; f32 _z;};
    f32 elements[3];
} Korl_Math_V3f32;
const Korl_Math_V3f32 KORL_MATH_V3F32_ONE     = { 1,  1,  1};
const Korl_Math_V3f32 KORL_MATH_V3F32_ZERO    = { 0,  0,  0};
const Korl_Math_V3f32 KORL_MATH_V3F32_X       = { 1,  0,  0};
const Korl_Math_V3f32 KORL_MATH_V3F32_Y       = { 0,  1,  0};
const Korl_Math_V3f32 KORL_MATH_V3F32_Z       = { 0,  0,  1};
const Korl_Math_V3f32 KORL_MATH_V3F32_MINUS_X = {-1,  0,  0};
const Korl_Math_V3f32 KORL_MATH_V3F32_MINUS_Y = { 0, -1,  0};
const Korl_Math_V3f32 KORL_MATH_V3F32_MINUS_Z = { 0,  0, -1};
typedef union Korl_Math_V4f32
{
    struct { f32 x, y, z, w; };
    struct { Korl_Math_V2f32 xy, zw; };
    struct { Korl_Math_V3f32 xyz; f32 _w; };
    f32 elements[4];
} Korl_Math_V4f32;
const Korl_Math_V4f32 KORL_MATH_V4F32_ONE  = {1, 1, 1, 1};
const Korl_Math_V4f32 KORL_MATH_V4F32_ZERO = {0, 0, 0, 0};
typedef union Korl_Math_Quaternion
{
    struct { f32 x, y, z, w; };
    struct { Korl_Math_V3f32 xyz; f32 _w; };
    Korl_Math_V4f32 v4;// we do this instead of just `typedef V4f32 Quaternion` because in C++ we _must_ differentiate `quaternion operator*` with `v4f32 operator*`
    f32 elements[4];
} Korl_Math_Quaternion;
const Korl_Math_Quaternion KORL_MATH_QUATERNION_IDENTITY = {0, 0, 0, 1};
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
const Korl_Math_M4f32 KORL_MATH_M4F32_IDENTITY = {
    { {1,0,0,0}
    , {0,1,0,0}
    , {0,0,1,0}
    , {0,0,0,1} }};
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
typedef struct Korl_Math_Aabb2f32
{
    Korl_Math_V2f32 min;
    Korl_Math_V2f32 max;
} Korl_Math_Aabb2f32;
const Korl_Math_Aabb2f32 KORL_MATH_AABB2F32_EMPTY = {{ KORL_F32_MAX, KORL_F32_MAX}
                                                    ,{-KORL_F32_MAX,-KORL_F32_MAX}};
typedef struct Korl_Math_Aabb3f32
{
    Korl_Math_V3f32 min;
    Korl_Math_V3f32 max;
} Korl_Math_Aabb3f32;
const Korl_Math_Aabb3f32 KORL_MATH_AABB3F32_EMPTY = {{ KORL_F32_MAX, KORL_F32_MAX, KORL_F32_MAX}
                                                    ,{-KORL_F32_MAX,-KORL_F32_MAX,-KORL_F32_MAX}};
typedef struct Korl_Math_Rng_WichmannHill
{
    u16 seed[3];
} Korl_Math_Rng_WichmannHill;
//@TODO: create a Korl_Math_Transform3d, which has the capability of privately managing translation/scale/rotation, and lazy-generating/caching a transformation matrix for the user at-will via a "public" API
korl_internal inline u64 korl_math_kilobytes(u64 x);
korl_internal inline u64 korl_math_megabytes(u64 x);
korl_internal inline u64 korl_math_gigabytes(u64 x);
korl_internal inline u$ korl_math_nextHighestDivision(u$ value, u$ division);
korl_internal inline u$ korl_math_roundUpPowerOf2(u$ value, u$ powerOf2Multiple);
korl_internal inline bool korl_math_isNearlyZero(f32 x);
korl_internal inline bool korl_math_isNearlyEqualEpsilon(f32 fA, f32 fB, f32 epsilon);
korl_internal inline bool korl_math_isNearlyEqual(f32 fA, f32 fB);/** calls korl_math_isNearlyEqualEpsilon(fA, fB, 1e-5f) */
korl_internal inline f32 korl_math_f32_positive(f32 x);
korl_internal inline f32 korl_math_f32_nan(void);
korl_internal inline f32 korl_math_lerp(f32 from, f32 to, f32 factor);
//KORL-PERFORMANCE-000-000-051: math: this sphere mesh generating API is crappy and does not generate vertex indices, so we will end up duplicating a ton of vertices
korl_internal u$ korl_math_generateMeshSphereVertexCount(u32 latitudeSegments, u32 longitudeSegments);
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
korl_internal void korl_math_generateMeshSphere(f32 radius, u32 latitudeSegments, u32 longitudeSegments, Korl_Math_V3f32* o_vertexPositions, u$ vertexPositionByteStride, Korl_Math_V2f32* o_vertexTextureNormals, u$ vertexTextureNormalByteStride);
/* RNG ************************************************************************/
korl_internal inline Korl_Math_Rng_WichmannHill korl_math_rng_wichmannHill_new(u16 seed0, u16 seed1, u16 seed2);
korl_internal inline Korl_Math_Rng_WichmannHill korl_math_rng_wichmannHill_new_u64(u64 seed);
korl_internal inline f32 korl_math_rng_wichmannHill_f32_0_1(Korl_Math_Rng_WichmannHill*const context);
korl_internal inline f32 korl_math_rng_wichmannHill_f32_range(Korl_Math_Rng_WichmannHill*const context, f32 range0, f32 range1);
/* V2u32 **********************************************************************/
korl_internal Korl_Math_V2u32 korl_math_v2u32_addScalar(Korl_Math_V2u32 v, u32 scalar);
korl_internal Korl_Math_V2u32 korl_math_v2u32_divide(Korl_Math_V2u32 vA, Korl_Math_V2u32 vB);
/* V2f32 **********************************************************************/
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
/* V3f32 **********************************************************************/
korl_internal f32 korl_math_v3f32_magnitude(const Korl_Math_V3f32*const v);
korl_internal f32 korl_math_v3f32_magnitudeSquared(const Korl_Math_V3f32*const v);
korl_internal Korl_Math_V3f32 korl_math_v3f32_normal(Korl_Math_V3f32 v);
korl_internal Korl_Math_V3f32 korl_math_v3f32_normalKnownMagnitude(Korl_Math_V3f32 v, f32 magnitude);
korl_internal Korl_Math_V3f32 korl_math_v3f32_cross(Korl_Math_V3f32 vA, Korl_Math_V3f32 vB);
korl_internal Korl_Math_V3f32 korl_math_v3f32_tripleProduct(Korl_Math_V3f32 vA, Korl_Math_V3f32 vB, Korl_Math_V3f32 vC);// (vA x vB) x vC
korl_internal f32 korl_math_v3f32_dot(Korl_Math_V3f32 vA, Korl_Math_V3f32 vB);
korl_internal f32 korl_math_v3f32_radiansBetween(Korl_Math_V3f32 vA, Korl_Math_V3f32 vB);
korl_internal Korl_Math_V3f32 korl_math_v3f32_project(Korl_Math_V3f32 vA, Korl_Math_V3f32 vB, bool vbIsNormalized);
korl_internal Korl_Math_V3f32 korl_math_v3f32_add(Korl_Math_V3f32 vA, Korl_Math_V3f32 vB);
// korl_internal Korl_Math_V3f32 korl_math_v3f32_addScalar(Korl_Math_V3f32 v, f32 scalar);
korl_internal Korl_Math_V3f32 korl_math_v3f32_subtract(Korl_Math_V3f32 vA, Korl_Math_V3f32 vB);
// korl_internal Korl_Math_V3f32 korl_math_v3f32_subtractScalar(Korl_Math_V3f32 v, f32 scalar);
korl_internal Korl_Math_V3f32 korl_math_v3f32_multiply(Korl_Math_V3f32 vA, Korl_Math_V3f32 vB);
korl_internal Korl_Math_V3f32 korl_math_v3f32_multiplyScalar(Korl_Math_V3f32 v, f32 scalar);
korl_internal Korl_Math_V3f32 korl_math_v3f32_divide(Korl_Math_V3f32 vA, Korl_Math_V3f32 vB);
korl_internal Korl_Math_V3f32 korl_math_v3f32_divideScalar(Korl_Math_V3f32 v, f32 scalar);
korl_internal void korl_math_v3f32_assignAdd(Korl_Math_V3f32*const vA, Korl_Math_V3f32 vB);
korl_internal void korl_math_v3f32_assignAddScalar(Korl_Math_V3f32*const v, f32 scalar);
korl_internal void korl_math_v3f32_assignSubtract(Korl_Math_V3f32*const vA, Korl_Math_V3f32 vB);
korl_internal void korl_math_v3f32_assignSubtractScalar(Korl_Math_V3f32*const v, f32 scalar);
korl_internal void korl_math_v3f32_assignMultiply(Korl_Math_V3f32*const vA, Korl_Math_V3f32 vB);
korl_internal void korl_math_v3f32_assignMultiplyScalar(Korl_Math_V3f32*const v, f32 scalar);
// korl_internal Korl_Math_V3f32 korl_math_v3f32_min(Korl_Math_V3f32 vA, Korl_Math_V3f32 vB);
// korl_internal Korl_Math_V3f32 korl_math_v3f32_max(Korl_Math_V3f32 vA, Korl_Math_V3f32 vB);
/* V4u8 ***********************************************************************/
korl_internal Korl_Math_V4u8 korl_math_v4u8_lerp(Korl_Math_V4u8 from, Korl_Math_V4u8 to, f32 factor);
/* V4f32 **********************************************************************/
korl_internal f32 korl_math_v4f32_magnitude(const Korl_Math_V4f32*const v);
korl_internal f32 korl_math_v4f32_magnitudeSquared(const Korl_Math_V4f32*const v);
korl_internal Korl_Math_V4f32 korl_math_v4f32_normal(Korl_Math_V4f32 v);
korl_internal Korl_Math_V4f32 korl_math_v4f32_normalKnownMagnitude(Korl_Math_V4f32 v, f32 magnitude);
korl_internal f32 korl_math_v4f32_dot(const Korl_Math_V4f32*const vA, const Korl_Math_V4f32*const vB);
korl_internal Korl_Math_V4f32 korl_math_v4f32_add(Korl_Math_V4f32 vA, Korl_Math_V4f32 vB);
korl_internal Korl_Math_V4f32 korl_math_v4f32_addScalar(Korl_Math_V4f32 v, f32 scalar);
korl_internal Korl_Math_V4f32 korl_math_v4f32_subtract(Korl_Math_V4f32 vA, Korl_Math_V4f32 vB);
korl_internal Korl_Math_V4f32 korl_math_v4f32_subtractScalar(Korl_Math_V4f32 v, f32 scalar);
korl_internal Korl_Math_V4f32 korl_math_v4f32_multiply(Korl_Math_V4f32 vA, Korl_Math_V4f32 vB);
korl_internal Korl_Math_V4f32 korl_math_v4f32_multiplyScalar(Korl_Math_V4f32 v, f32 scalar);
korl_internal Korl_Math_V4f32 korl_math_v4f32_divide(Korl_Math_V4f32 vA, Korl_Math_V4f32 vB);
korl_internal Korl_Math_V4f32 korl_math_v4f32_divideScalar(Korl_Math_V4f32 v, f32 scalar);
korl_internal Korl_Math_V4f32* korl_math_v4f32_assignAdd(Korl_Math_V4f32*const vA, Korl_Math_V4f32 vB);
korl_internal Korl_Math_V4f32* korl_math_v4f32_assignAddScalar(Korl_Math_V4f32*const v, f32 scalar);
korl_internal Korl_Math_V4f32* korl_math_v4f32_assignSubtract(Korl_Math_V4f32*const vA, Korl_Math_V4f32 vB);
korl_internal Korl_Math_V4f32* korl_math_v4f32_assignSubtractScalar(Korl_Math_V4f32*const v, f32 scalar);
korl_internal Korl_Math_V4f32* korl_math_v4f32_assignMultiply(Korl_Math_V4f32*const vA, Korl_Math_V4f32 vB);
korl_internal Korl_Math_V4f32* korl_math_v4f32_assignMultiplyScalar(Korl_Math_V4f32*const v, f32 scalar);
korl_internal Korl_Math_V4f32* korl_math_v4f32_assignDivide(Korl_Math_V4f32*const vA, Korl_Math_V4f32 vB);
korl_internal Korl_Math_V4f32* korl_math_v4f32_assignDivideScalar(Korl_Math_V4f32*const v, f32 scalar);
// korl_internal Korl_Math_V4f32 korl_math_v4f32_min(Korl_Math_V4f32 vA, Korl_Math_V4f32 vB);
// korl_internal Korl_Math_V4f32 korl_math_v4f32_max(Korl_Math_V4f32 vA, Korl_Math_V4f32 vB);
/* Quaternion *****************************************************************/
/** \return the magnitude of \c q */
korl_internal f32                  korl_math_quaternion_normalize(Korl_Math_Quaternion* q);
korl_internal Korl_Math_Quaternion korl_math_quaternion_fromAxisRadians(Korl_Math_V3f32 axis, f32 radians, bool axisIsNormalized);
korl_internal Korl_Math_Quaternion korl_math_quaternion_fromVector(Korl_Math_V3f32 forward, Korl_Math_V3f32 worldForward, Korl_Math_V3f32 worldUp);
korl_internal Korl_Math_Quaternion korl_math_quaternion_multiply(Korl_Math_Quaternion qA, Korl_Math_Quaternion qB);
korl_internal Korl_Math_Quaternion korl_math_quaternion_hamilton(Korl_Math_Quaternion qA, Korl_Math_Quaternion qB);
korl_internal Korl_Math_Quaternion korl_math_quaternion_conjugate(Korl_Math_Quaternion q);
korl_internal Korl_Math_V2f32 korl_math_quaternion_transformV2f32(Korl_Math_Quaternion q, Korl_Math_V2f32 v, bool qIsNormalized);
korl_internal Korl_Math_V3f32 korl_math_quaternion_transformV3f32(Korl_Math_Quaternion q, Korl_Math_V3f32 v, bool qIsNormalized);
/* M4f32 **********************************************************************/
korl_internal Korl_Math_M4f32 korl_math_makeM4f32_rotate(Korl_Math_Quaternion qRotation);
korl_internal Korl_Math_M4f32 korl_math_makeM4f32_rotateTranslate(Korl_Math_Quaternion qRotation, Korl_Math_V3f32 vTranslation);
korl_internal Korl_Math_M4f32 korl_math_makeM4f32_rotateScaleTranslate(Korl_Math_Quaternion qRotation, Korl_Math_V3f32 vScale, Korl_Math_V3f32 vTranslation);
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
korl_internal Korl_Math_Aabb3f32 korl_math_aabb3f32_fromPoints(f32 p0x, f32 p0y, f32 p0z, f32 p1x, f32 p1y, f32 p1z);
// korl_internal Korl_Math_Aabb3f32 korl_math_aabb3f32_fromExpandedV3(Korl_Math_Aabb3f32 aabb, f32 expandX, f32 expandY, f32 expandZ);
// korl_internal Korl_Math_Aabb3f32 korl_math_aabb3f32_fromExpandedV3(Korl_Math_V3f32 v, f32 expandX, f32 expandY, f32 expandZ);
korl_internal void               korl_math_aabb3f32_expand(Korl_Math_Aabb3f32*const aabb, f32 expandXYZ);
korl_internal Korl_Math_V3f32    korl_math_aabb3f32_size(Korl_Math_Aabb3f32 aabb);
korl_internal bool               korl_math_aabb3f32_contains(Korl_Math_Aabb3f32 aabb, f32 x, f32 y, f32 z);
korl_internal bool               korl_math_aabb3f32_containsV3f32(Korl_Math_Aabb3f32 aabb, Korl_Math_V3f32 v);
korl_internal Korl_Math_Aabb3f32 korl_math_aabb3f32_union(Korl_Math_Aabb3f32 aabbA, Korl_Math_Aabb3f32 aabbB);
korl_internal bool               korl_math_aabb3f32_areIntersecting(Korl_Math_Aabb3f32 aabbA, Korl_Math_Aabb3f32 aabbB);
korl_internal void               korl_math_aabb3f32_addPoint(Korl_Math_Aabb3f32*const aabb, f32* point3d);
korl_internal void               korl_math_aabb3f32_addPointV3(Korl_Math_Aabb3f32*const aabb, Korl_Math_V3f32 point);
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
korl_internal Korl_Math_V3f32& operator*=(Korl_Math_V3f32& v, Korl_Math_Quaternion q);
korl_internal Korl_Math_Quaternion operator*(Korl_Math_Quaternion qA, Korl_Math_Quaternion qB);
korl_internal Korl_Math_M4f32 operator*(const Korl_Math_M4f32& mA, const Korl_Math_M4f32& mB);
korl_internal Korl_Math_V2f32 operator*(const Korl_Math_M4f32& m, const Korl_Math_V2f32& v);
korl_internal Korl_Math_V3f32 operator*(const Korl_Math_M4f32& m, const Korl_Math_V3f32& v);
korl_internal Korl_Math_Aabb2f32& operator+=(Korl_Math_Aabb2f32& aabb, Korl_Math_V2f32 v);
korl_internal Korl_Math_Aabb3f32& operator+=(Korl_Math_Aabb3f32& aabb, Korl_Math_V3f32 v);
#endif//__cplusplus
