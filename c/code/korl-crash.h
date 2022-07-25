#pragma once
#include "korl-globalDefines.h"
#include "korl-interface-platform.h"
korl_internal void korl_crash_initialize(void);
#if !defined(KORL_DEFINED_INTERFACE_PLATFORM_API)
#define KORL_DEFINED_INTERFACE_PLATFORM_API
/** \note DO NOT CALL THIS!  Use \c korl_assert instead! */
korl_internal KORL_PLATFORM_ASSERT_FAILURE(_korl_crash_assertConditionFailed);
#endif// !defined(KORL_DEFINED_INTERFACE_PLATFORM_API)
