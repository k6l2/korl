#pragma once
#include "korl-globalDefines.h"
korl_internal u8  korl_checkCast_u$_to_u8(u$ x);
korl_internal u16 korl_checkCast_u$_to_u16(u$ x);
korl_internal u32 korl_checkCast_u$_to_u32(u$ x);
korl_internal i8  korl_checkCast_u$_to_i8(u$ x);
korl_internal i16 korl_checkCast_u$_to_i16(u$ x);
korl_internal i32 korl_checkCast_u$_to_i32(u$ x);
korl_internal i$  korl_checkCast_u$_to_i$(u$ x);
korl_internal u$  korl_checkCast_i$_to_u$(i$ x);
korl_internal u8  korl_checkCast_i$_to_u8(i$ x);
korl_internal u16 korl_checkCast_i$_to_u16(i$ x);
korl_internal u32 korl_checkCast_i$_to_u32(i$ x);
korl_internal f32 korl_checkCast_i$_to_f32(i$ x);
korl_internal wchar_t*       korl_checkCast_pu16_to_pwchar(u16* x);
korl_internal const wchar_t* korl_checkCast_cpu16_to_cpwchar(const u16* x);
korl_internal const char*    korl_checkCast_cpu8_to_cpchar(const u8* x);
korl_internal u64 korl_checkCast_cvoidp_to_u64(const void* x);
