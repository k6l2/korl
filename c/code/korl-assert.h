#pragma once
#include "korl-globalDefines.h"
#include "korl-interface-platform.h"
korl_internal void korl_assert_initialize(void);
/** \note DO NOT CALL THIS!  Use \c korl_assert instead! */
korl_internal KORL_PLATFORM_ASSERT_FAILURE(korl_assertConditionFailed);
