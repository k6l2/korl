#pragma once
#include "korl-globalDefines.h"
korl_internal void korl_crash_initialize(void);
/** \note DO NOT CALL THIS!  Use \c korl_assert instead! */
korl_internal KORL_PLATFORM_ASSERT_FAILURE(_korl_crash_assertConditionFailed);
