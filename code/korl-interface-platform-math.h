#pragma once
#define KORL_FUNCTION_korl_math_round_f32_to_u8(name)  u8   name(f32 x)
#define KORL_FUNCTION_korl_math_round_f32_to_u32(name) u32  name(f32 x)
#define KORL_FUNCTION_korl_math_round_f32_to_i32(name) i32  name(f32 x)
#define KORL_FUNCTION_korl_math_round_f64_to_i64(name) i64  name(f64 x)
#define KORL_FUNCTION_korl_math_f32_modulus(name)      f32  name(f32 numerator, f32 denominator)
#define KORL_FUNCTION_korl_math_sine(name)             f32  name(f32 x)
#define KORL_FUNCTION_korl_math_cosine(name)           f32  name(f32 x)
#define KORL_FUNCTION_korl_math_arcCosine(name)        f32  name(f32 x)
#define KORL_FUNCTION_korl_math_tangent(name)          f32  name(f32 x)
#define KORL_FUNCTION_korl_math_arcTangent(name)       f32  name(f32 numerator, f32 denominator)
#define KORL_FUNCTION_korl_math_f32_floor(name)        f32  name(f32 x)
#define KORL_FUNCTION_korl_math_f64_floor(name)        f64  name(f64 x)
#define KORL_FUNCTION_korl_math_f32_ceiling(name)      f32  name(f32 x)
#define KORL_FUNCTION_korl_math_f32_squareRoot(name)   f32  name(f32 x)
#define KORL_FUNCTION_korl_math_f32_exponential(name)  f32  name(f32 x)
#define KORL_FUNCTION_korl_math_f64_exponential(name)  f64  name(f64 x)
#define KORL_FUNCTION_korl_math_f32_power(name)        f32  name(f32 x, f32 exponent)
#define KORL_FUNCTION_korl_math_f64_power(name)        f64  name(f64 x, f64 exponent)
#define KORL_FUNCTION_korl_math_loadExponent(name)     f32  name(f32 x, i32 exponent)/** computes x*(2^exponent) */
#define KORL_FUNCTION_korl_math_f32_isNan(name)        bool name(f32 x)
#define KORL_FUNCTION_korl_math_exDecay(name)          f32  name(f32 from, f32 to, f32 lambdaFactor, f32 deltaTime)