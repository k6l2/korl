#include "korl-math.h"
#include <math.h>
korl_internal inline u64 korl_math_kilobytes(u64 x)
{
    return 1024 * x;
}
korl_internal inline u64 korl_math_megabytes(u64 x)
{
    return 1024 * korl_math_kilobytes(x);
}
korl_internal inline uintptr_t korl_math_roundUpPowerOf2(
    uintptr_t value, uintptr_t powerOf2Multiple)
{
    /* derived from this source: https://stackoverflow.com/a/9194117 */
    // ensure multiple is non-zero
    korl_assert(powerOf2Multiple);
    // ensure multiple is a power of two
    korl_assert((powerOf2Multiple & (powerOf2Multiple - 1)) == 0);
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
    result.xyz.x = vA->xyz.y*vB->xyz.z - vA->xyz.z*vB->xyz.y;
    result.xyz.y = vA->xyz.z*vB->xyz.x - vA->xyz.x*vB->xyz.z;
    result.xyz.z = vA->xyz.x*vB->xyz.y - vA->xyz.y*vB->xyz.x;
    return result;
}
korl_internal f32 korl_math_v3f32_dot(const Korl_Math_V3f32*const vA, const Korl_Math_V3f32*const vB)
{
    return vA->elements[0] * vB->elements[0]
        +  vA->elements[1] * vB->elements[1]
        +  vA->elements[2] * vB->elements[2];
}
korl_internal Korl_Math_V3f32 korl_math_v3f32_subtract(Korl_Math_V3f32 vA, const Korl_Math_V3f32*const vB)
{
    vA.elements[0] -= vB->elements[0];
    vA.elements[1] -= vB->elements[1];
    vA.elements[2] -= vB->elements[2];
    return vA;
}
korl_internal Korl_Math_M4f32 korl_math_m4f32_transpose(const Korl_Math_M4f32*const m)
{
    KORL_ZERO_STACK(Korl_Math_M4f32, result);
    for(unsigned r = 0; r < 4; r++)
        for(unsigned c = 0; c < 4; c++)
            result.rows[r].elements[c] = m->rows[c].elements[r];
    return result;
}
korl_internal Korl_Math_M4f32 korl_math_m4f32_projectionFov(
    f32 horizontalFovDegrees, f32 viewportWidthOverHeight, 
    f32 clipNear, f32 clipFar)
{
    KORL_ZERO_STACK(Korl_Math_M4f32, result);
    korl_assert(!korl_math_isNearlyZero(horizontalFovDegrees));
    korl_assert(!korl_math_isNearlyEqual(clipNear, clipFar) && clipNear < clipFar);
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
    result.rc.r0c0 = 1.f / tanHalfFov;
    result.rc.r1c1 = viewportWidthOverHeight / tanHalfFov;
    result.rc.r2c2 =             -clipFar / (clipNear - clipFar);
    result.rc.r2c3 = (clipNear * clipFar) / (clipNear - clipFar);
    result.rc.r3c2 = 1;
    return result;
}
korl_internal Korl_Math_M4f32 korl_math_m4f32_lookAt(
    const Korl_Math_V3f32*const positionEye, 
    const Korl_Math_V3f32*const positionTarget, 
    const Korl_Math_V3f32*const worldUpNormal)
{
    /* @speed: could potentially just compile this out for "release" builds, but 
               like who actually cares??  The performance gain from doing so is 
               most likely not at all worth it for all I know.  PRE-MATURE OPT! */
    /* sanity check to ensure that worldUpNormal is, in fact, normalized! */
    korl_assert(korl_math_isNearlyEqual(korl_math_v3f32_magnitudeSquared(worldUpNormal), 1));
    Korl_Math_M4f32 result = KORL_MATH_M4F32_IDENTITY;
    /* Even though this author uses "column-major" memory layout for matrices 
        for some reason, which makes everything really fucking confusing most of 
        the time, this still does a good job explaining a useful implementation 
        of the view matrix: https://www.3dgep.com/understanding-the-view-matrix/ */
    /* our camera eye-space will be left-handed, with the camera looking down 
        the +Z axis, and +Y being UP */
    const Korl_Math_V3f32 cameraZ = korl_math_v3f32_normal(korl_math_v3f32_subtract(*positionTarget, positionEye));
    const Korl_Math_V3f32 cameraX = korl_math_v3f32_cross(&cameraZ, worldUpNormal);
    const Korl_Math_V3f32 cameraY = korl_math_v3f32_cross(&cameraZ, &cameraX);
    /* the view matrix is simply the inverse of the camera transform, and since 
        the camera transform is orthonormal, we can simply transpose the 
        elements of the rotation matrix */
    /* additionally, we can combine the inverse camera translation into the same 
        matrix as the inverse rotation by manually doing the row-column dot 
        products ourselves, and putting them in the 4th column */
    result.rc.r0c0 = cameraX.xyz.x; result.rc.r0c1 = cameraX.xyz.y; result.rc.r0c2 = cameraX.xyz.z; result.rc.r0c3 = -korl_math_v3f32_dot(&cameraX, positionEye);
    result.rc.r1c0 = cameraY.xyz.x; result.rc.r1c1 = cameraY.xyz.y; result.rc.r1c2 = cameraY.xyz.z; result.rc.r1c3 = -korl_math_v3f32_dot(&cameraY, positionEye);
    result.rc.r2c0 = cameraZ.xyz.x; result.rc.r2c1 = cameraZ.xyz.y; result.rc.r2c2 = cameraZ.xyz.z; result.rc.r2c3 = -korl_math_v3f32_dot(&cameraZ, positionEye);
    return result;
}
