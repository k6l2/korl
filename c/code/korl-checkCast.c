#include "korl-checkCast.h"
#include "korl-assert.h"
korl_internal u16 korl_checkCast_u32_to_u16(u32 x)
{
    korl_assert(x < 0xFFFF);
    return (u16)x;
}
