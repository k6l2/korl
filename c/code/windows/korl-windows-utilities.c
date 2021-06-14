#include "korl-windows-utilities.h"
korl_internal DWORD korl_windows_sizet_to_dword(size_t value)
{
    _STATIC_ASSERT(sizeof(DWORD) == 4);
    korl_assert(value <= 0xFFFFFFFF);
    return (DWORD)value;
}
korl_internal i32 korl_windows_dword_to_i32(DWORD value)
{
    _STATIC_ASSERT(sizeof(DWORD) == 4);
    korl_assert(value <= 0x7FFFFFFF);
    return (i32)value;
}
