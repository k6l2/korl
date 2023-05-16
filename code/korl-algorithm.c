#include "korl-algorithm.h"
korl_internal KORL_FUNCTION_korl_algorithm_sort_quick(korl_algorithm_sort_quick)
{
    qsort(array, arraySize, arrayStride, compare);
}
#ifdef KORL_PLATFORM_WINDOWS
    /* in case the reader is wondering, the only reason why any of this 
        Windows-specific shit exists is simply because some dickhead at 
        microshaft thought it was a great idea to change the function 
        signature of the compare callback to qsort_s such that the context 
        is the first parameter instead of the last */
    typedef struct _Korl_Windows_Algorithm_Sort_Quick_Context
    {
        void* context;
        fnSig_korl_algorithm_compare_context* compare;
    } _Korl_Windows_Algorithm_Sort_Quick_Context;
    int _korl_windows_algorithm_sort_quick_compareWrapper(void* context, const void* a, const void* b)
    {
        _Korl_Windows_Algorithm_Sort_Quick_Context* contextWrapper = context;
        return contextWrapper->compare(a, b, contextWrapper->context);
    }
#endif
korl_internal KORL_FUNCTION_korl_algorithm_sort_quick_context(korl_algorithm_sort_quick_context)
{
    #ifdef KORL_PLATFORM_WINDOWS
        _Korl_Windows_Algorithm_Sort_Quick_Context windows_crt_is_a_piece_of_shit_wrapper = {context, compare};
        qsort_s(array, arraySize, arrayStride, _korl_windows_algorithm_sort_quick_compareWrapper, &windows_crt_is_a_piece_of_shit_wrapper);
    #else
        qsort_s(array, arraySize, arrayStride, compare, context);
    #endif
}
