#include "korl-windows-utilities.h"
korl_internal DWORD korl_windows_sizet_to_dword(size_t value)
{
    _STATIC_ASSERT(sizeof(DWORD) == 4);
    korl_assert(value <= 0xFFFFFFFF);
    return (DWORD)value;
}
