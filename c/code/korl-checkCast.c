#include "korl-checkCast.h"
#pragma warning(push)
#pragma warning(disable:4755)/* in optimized builds, it is possible for the compiler to find conditions in which one of the paths of these checks will never be executed, and that is fine by me (for now) */
korl_internal u16 korl_checkCast_u32_to_u16(u32 x)
{
    korl_assert(x < 0xFFFF);
    return KORL_C_CAST(u16, x);
}
korl_internal u8 korl_checkCast_u$_to_u8(u$ x)
{
    korl_assert(x <= 0xFF);
    return KORL_C_CAST(u8, x);
}
korl_internal u16 korl_checkCast_u$_to_u16(u$ x)
{
    korl_assert(x <= 0xFFFF);
    return KORL_C_CAST(u16, x);
}
korl_internal u32 korl_checkCast_u$_to_u32(u$ x)
{
    korl_assert(x <= 0xFFFFFFFF);
    return KORL_C_CAST(u32, x);
}
korl_internal i$ korl_checkCast_u$_to_i$(u$ x)
{
    korl_shared_const u$ MAX_I$ = ((u$)(-1)) >> 1;
    korl_assert(x <= MAX_I$);
    return KORL_C_CAST(i$, x);
}
korl_internal u$ korl_checkCast_i$_to_u$(i$ x)
{
    korl_assert(x >= 0);
    return KORL_C_CAST(u$, x);
}
korl_internal u16 korl_checkCast_i$_to_u16(i$ x)
{
    korl_assert(x >= 0);
    korl_assert(x <= 0xFFFF);
    return KORL_C_CAST(u16, x);
}
korl_internal u32 korl_checkCast_i$_to_u32(i$ x)
{
    korl_assert(x >= 0);
    korl_assert(x <= 0xFFFFFFFF);
    return KORL_C_CAST(u32, x);
}
korl_internal f32 korl_checkCast_i$_to_f32(i$ x)
{
    const f32 result = KORL_C_CAST(f32, x);
    korl_assert(KORL_C_CAST(i$, result) == x);
    return result;
}
#pragma warning(pop)
