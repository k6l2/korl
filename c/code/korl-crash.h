#pragma once
#include "korl-globalDefines.h"
#include "korl-interface-platform.h"
korl_internal void korl_crash_initialize(void);
#if !defined(KORL_DEFINED_PLATFORM_MODULE_CRASH)
#define KORL_DEFINED_PLATFORM_MODULE_CRASH
/** \note DO NOT CALL THIS!  Use \c korl_assert instead! */
korl_internal KORL_PLATFORM_ASSERT_FAILURE(_korl_crash_assertConditionFailed);
#endif// !defined(KORL_DEFINED_PLATFORM_MODULE_CRASH)
