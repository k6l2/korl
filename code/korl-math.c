#include "korl-math.h"
#include <math.h>
korl_internal inline KORL_FUNCTION_korl_math_round_f32_to_u8(korl_math_round_f32_to_u8)
{
    /** maybe in the future, we can have a general solution? 
     * https://stackoverflow.com/a/497079 */
    x = roundf(x);
    korl_assert(x >= 0.0f);
    korl_assert(x <= KORL_C_CAST(f32, KORL_U8_MAX));
    return KORL_C_CAST(u8, x);
}
korl_internal inline KORL_FUNCTION_korl_math_round_f32_to_u32(korl_math_round_f32_to_u32)
{
    /** maybe in the future, we can have a general solution? 
     * https://stackoverflow.com/a/497079 */
    x = roundf(x);
    korl_assert(x >= 0.0f);
    korl_assert(x <= KORL_C_CAST(f32, KORL_U32_MAX));
    return KORL_C_CAST(u32, x);
}
korl_internal inline KORL_FUNCTION_korl_math_round_f32_to_i32(korl_math_round_f32_to_i32)
{
    /** maybe in the future, we can have a general solution? 
     * https://stackoverflow.com/a/497079 */
    x = roundf(x);
    korl_assert(x >= KORL_C_CAST(f32, KORL_I32_MIN));
    korl_assert(x <= KORL_C_CAST(f32, KORL_I32_MAX));
    return KORL_C_CAST(i32, x);
}
korl_internal inline KORL_FUNCTION_korl_math_round_f64_to_i64(korl_math_round_f64_to_i64)
{
    return KORL_C_CAST(i64, round(x));
}
korl_internal inline KORL_FUNCTION_korl_math_f32_modulus(korl_math_f32_modulus)
{
    return fmodf(numerator, denominator);
}
korl_internal inline KORL_FUNCTION_korl_math_sine(korl_math_sine)
{
    return sinf(x);
}
korl_internal inline KORL_FUNCTION_korl_math_arcSine(korl_math_arcSine)
{
    return asinf(x);
}
korl_internal inline KORL_FUNCTION_korl_math_cosine(korl_math_cosine)
{
    return cosf(x);
}
korl_internal inline KORL_FUNCTION_korl_math_arcCosine(korl_math_arcCosine)
{
    if(korl_math_isNearlyEqual(x, 1.f))
        x = 1.f;
    if(korl_math_isNearlyEqual(x, -1.f))
        x = -1.f;
    korl_assert(-1.f <= x && x <= 1.f);
    return acosf(x);
}
korl_internal inline KORL_FUNCTION_korl_math_tangent(korl_math_tangent)
{
    return tanf(x);
}
korl_internal inline KORL_FUNCTION_korl_math_arcTangent(korl_math_arcTangent)
{
    return atan2f(numerator, denominator);
}
korl_internal inline KORL_FUNCTION_korl_math_f32_floor(korl_math_f32_floor)
{
    return floorf(x);
}
korl_internal inline KORL_FUNCTION_korl_math_f64_floor(korl_math_f64_floor)
{
    return floor(x);
}
korl_internal inline KORL_FUNCTION_korl_math_f32_ceiling(korl_math_f32_ceiling)
{
    return ceilf(x);
}
korl_internal inline KORL_FUNCTION_korl_math_f32_squareRoot(korl_math_f32_squareRoot)
{
    return sqrtf(x);
}
korl_internal inline KORL_FUNCTION_korl_math_f32_exponential(korl_math_f32_exponential)
{
    return expf(x);
}
korl_internal inline KORL_FUNCTION_korl_math_f64_exponential(korl_math_f64_exponential)
{
    return exp(x);
}
korl_internal inline KORL_FUNCTION_korl_math_f32_power(korl_math_f32_power)
{
    return powf(x, exponent);
}
korl_internal inline KORL_FUNCTION_korl_math_f64_power(korl_math_f64_power)
{
    return pow(x, exponent);
}
korl_internal inline KORL_FUNCTION_korl_math_loadExponent(korl_math_loadExponent)
{
    return ldexpf(x, exponent);
}
korl_internal inline KORL_FUNCTION_korl_math_f32_isNan(korl_math_f32_isNan)
{
    return 0 != _isnanf(x);
}
korl_internal inline KORL_FUNCTION_korl_math_exDecay(korl_math_exDecay)
{
    // good resource on this:  
    //  https://www.rorydriscoll.com/2016/03/07/frame-rate-independent-damping-using-lerp/
    return korl_math_f32_lerp(to, from, expf(-lambdaFactor * deltaTime));
}
