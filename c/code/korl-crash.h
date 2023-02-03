#pragma once
#include "korl-globalDefines.h"
#include "korl-interface-platform.h"
korl_internal void korl_crash_initialize(void);
//KORL-ISSUE-000-000-120: interface-platform: remove KORL_DEFINED_INTERFACE_PLATFORM_API; this it just messy imo; if there is clear separation of code that should only exist in the platform layer, then we should probably just physically separate it out into separate source file(s)
#if !defined(KORL_DEFINED_INTERFACE_PLATFORM_API)
    /** \note DO NOT CALL THIS!  Use \c korl_assert instead! */
    korl_internal KORL_FUNCTION__korl_crash_assertConditionFailed(_korl_crash_assertConditionFailed);
#endif// !defined(KORL_DEFINED_INTERFACE_PLATFORM_API)
