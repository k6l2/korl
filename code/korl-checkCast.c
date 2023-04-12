#include "korl-checkCast.h"
#include "korl-crash.h"
#pragma warning(push)
#pragma warning(disable:4755)/* in optimized builds, it is possible for the compiler to find conditions in which one of the paths of these checks will never be executed, and that is fine by me (for now) */
korl_internal u8 korl_checkCast_u$_to_u8(u$ x)
{
    korl_assert(x <= KORL_U8_MAX);
    return KORL_C_CAST(u8, x);
}
korl_internal u16 korl_checkCast_u$_to_u16(u$ x)
{
    korl_assert(x <= KORL_U16_MAX);
    return KORL_C_CAST(u16, x);
}
korl_internal u32 korl_checkCast_u$_to_u32(u$ x)
{
    korl_assert(x <= KORL_U32_MAX);
    return KORL_C_CAST(u32, x);
}
korl_internal i8 korl_checkCast_u$_to_i8(u$ x)
{
    korl_assert(x <= KORL_I8_MAX);
    return KORL_C_CAST(i8, x);
}
korl_internal i16 korl_checkCast_u$_to_i16(u$ x)
{
    korl_assert(x <= KORL_I16_MAX);
    return KORL_C_CAST(i16, x);
}
korl_internal i32 korl_checkCast_u$_to_i32(u$ x)
{
    korl_assert(x <= KORL_I32_MAX);
    return KORL_C_CAST(i32, x);
}
korl_internal i$ korl_checkCast_u$_to_i$(u$ x)
{
    korl_shared_const u$ MAX_I$ = ((u$)(-1)) >> 1;
    korl_assert(x <= MAX_I$);
    return KORL_C_CAST(i$, x);
}
korl_internal f32 korl_checkCast_u$_to_f32(u$ x)
{
    const f32 result = KORL_C_CAST(f32, x);
    korl_assert(KORL_C_CAST(u$, result) == x);
    return result;
}
korl_internal u$ korl_checkCast_i$_to_u$(i$ x)
{
    korl_assert(x >= 0);
    return KORL_C_CAST(u$, x);
}
korl_internal u8 korl_checkCast_i$_to_u8(i$ x)
{
    korl_assert(x >= 0);
    korl_assert(x <= KORL_U8_MAX);
    return KORL_C_CAST(u8, x);
}
korl_internal u16 korl_checkCast_i$_to_u16(i$ x)
{
    korl_assert(x >= 0);
    korl_assert(x <= KORL_U16_MAX);
    return KORL_C_CAST(u16, x);
}
korl_internal u32 korl_checkCast_i$_to_u32(i$ x)
{
    korl_assert(x >= 0);
    korl_assert(x <= KORL_U32_MAX);
    return KORL_C_CAST(u32, x);
}
korl_internal i8  korl_checkCast_i$_to_i8(i$ x)
{
    korl_assert(x >= KORL_I8_MIN);
    korl_assert(x <= KORL_I8_MAX);
    return KORL_C_CAST(i8, x);
}
korl_internal i16 korl_checkCast_i$_to_i16(i$ x)
{
    korl_assert(x >= KORL_I16_MIN);
    korl_assert(x <= KORL_I16_MAX);
    return KORL_C_CAST(i16, x);
}
korl_internal i32 korl_checkCast_i$_to_i32(i$ x)
{
    korl_assert(x >= KORL_I32_MIN);
    korl_assert(x <= KORL_I32_MAX);
    return KORL_C_CAST(i32, x);
}
korl_internal f32 korl_checkCast_i$_to_f32(i$ x)
{
    const f32 result = KORL_C_CAST(f32, x);
    korl_assert(KORL_C_CAST(i$, result) == x);
    return result;
}
korl_internal i32 korl_checkCast_f32_to_i32(f32 x)
{
    const i32 result = KORL_C_CAST(i32, x);
    korl_assert(KORL_C_CAST(f32, result) == x);
    return result;
}
korl_internal u32 korl_checkCast_f32_to_u32(f32 x)
{
    const u32 result = KORL_C_CAST(u32, x);
    korl_assert(KORL_C_CAST(f32, result) == x);
    return result;
}
korl_internal wchar_t* korl_checkCast_pu16_to_pwchar(u16* x)
{
    wchar_t*const result = KORL_C_CAST(wchar_t*, x);
    korl_assert(sizeof(*x) == sizeof(*result));
    return result;
}
korl_internal const wchar_t* korl_checkCast_cpu16_to_cpwchar(const u16* x)
{
    const wchar_t*const result = KORL_C_CAST(const wchar_t*, x);
    korl_assert(sizeof(*x) == sizeof(*result));
    return result;
}
korl_internal const char* korl_checkCast_cpu8_to_cpchar(const u8* x)
{
    const char*const result = KORL_C_CAST(const char*, x);
    korl_assert(sizeof(*x) == sizeof(*result));
    return result;
}
korl_internal u64 korl_checkCast_cvoidp_to_u64(const void* x)
{
    korl_assert(sizeof(x) == sizeof(u64));
    return KORL_C_CAST(u64, x);
}
#pragma warning(pop)
