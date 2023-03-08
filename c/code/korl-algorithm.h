#pragma once
#include "korl-interface-platform.h"
#if !defined(KORL_DEFINED_INTERFACE_PLATFORM_API)// these API defined in the platform layer because they contain platform-specific code
    korl_internal KORL_FUNCTION_korl_algorithm_sort_quick(korl_algorithm_sort_quick);
    korl_internal KORL_FUNCTION_korl_algorithm_sort_quick_context(korl_algorithm_sort_quick_context);
#endif