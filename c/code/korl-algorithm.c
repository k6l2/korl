#include "korl-algorithm.h"
#include <stdlib.h>
korl_internal KORL_FUNCTION_korl_algorithm_sort_quick(korl_algorithm_sort_quick)
{
    qsort(array, arraySize, arrayStride, compare);
}
korl_internal KORL_FUNCTION_korl_algorithm_sort_quick_context(korl_algorithm_sort_quick_context)
{
    qsort_s(array, arraySize, arrayStride, compare, context);
}
