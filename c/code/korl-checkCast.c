#include "korl-checkCast.h"
#include "korl-assert.h"
korl_internal u16 korl_checkCast_u32_to_u16(u32 x)
{
    korl_assert(x < 0xFFFF);
    return (u16)x;
}
korl_internal i$ korl_checkCast_u$_to_i$(u$ x)
{
    korl_shared_const u$ MAX_I$ = ((u$)(-1)) >> 1;
    korl_assert(x <= MAX_I$);
    return (i$)x;
}
korl_internal u$ korl_checkCast_i$_to_u$(i$ x)
{
    korl_assert(x >= 0);
    return (u$)x;
}
korl_internal u32 korl_checkCast_i$_to_u32(i$ x)
{
    korl_assert(x >= 0);
    korl_assert(x <= 0xFFFFFFFF);
    return (u32)x;
}
