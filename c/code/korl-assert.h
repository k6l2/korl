#pragma once
#include "korl-globalDefines.h"
#define korl_assert(condition) \
    ((condition) ? (void)0 : korl_assertConditionFailed(L""#condition, __FILEW__, __LINE__))
/** \note DO NOT CALL THIS!  Use \c korl_assert instead! */
korl_internal void korl_assertConditionFailed(
    const wchar_t* conditionString, const wchar_t* cStringFileName, int lineNumber);
