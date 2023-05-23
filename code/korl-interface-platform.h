/**
 * See the comment block at the top of `korl-interface-platform-api.h` for 
 * instructions on how to add/remove KORL platform APIs.  This process is 
 * somewhat automated with C macros to reduce the amount of tedium.
 */
#pragma once
#include "korl-globalDefines.h"
#include <stdarg.h>//for va_list
#include "korl-interface-platform-assetCache.h"
#include "korl-interface-platform-resource.h"
#include "korl-interface-platform-time.h"
#include "korl-interface-platform-input.h"
#include "korl-interface-platform-gui.h"
#include "korl-interface-platform-gfx.h"
#include "korl-interface-platform-memory.h"
#include "korl-interface-platform-bluetooth.h"
#include "korl-interface-platform-sfx.h"
#include "korl-interface-platform-math.h"
#include "korl-interface-platform-network.h"
/* korl-log interface *********************************************************/
typedef enum KorlEnumLogLevel
    { KORL_LOG_LEVEL_ASSERT
    , KORL_LOG_LEVEL_ERROR
    , KORL_LOG_LEVEL_WARNING
    , KORL_LOG_LEVEL_INFO
    , KORL_LOG_LEVEL_VERBOSE
} KorlEnumLogLevel;
/** Do not call this function directly; use the \c korl_log macro instead! */
#define KORL_FUNCTION__korl_log_variadic(name) void  name(unsigned variadicArgumentCount, enum KorlEnumLogLevel logLevel\
                                                         ,const wchar_t* cStringFileName, const wchar_t* cStringFunctionName\
                                                         ,int lineNumber, const wchar_t* format, ...)
#define KORL_FUNCTION_korl_log_getBuffer(name) acu16 name(u$* out_loggedBytes)
/* korl-command interface *****************************************************/
#define KORL_FUNCTION_korl_command_callback(name)  void name(u$ parametersSize, acu8* parameters)
typedef KORL_FUNCTION_korl_command_callback(fnSig_korl_command_callback);
#define korl_command_register(utf8CommandName, callback) _korl_command_register(utf8CommandName, callback, KORL_RAW_CONST_UTF8(#callback))
/** use the \c korl_command_register convenience macro instead of this API directly! */
#define KORL_FUNCTION__korl_command_register(name) void name(acu8 utf8CommandName, fnSig_korl_command_callback* callback, acu8 utf8Callback)
#define KORL_FUNCTION_korl_command_invoke(name)    void name(acu8 rawUtf8, Korl_Memory_AllocatorHandle allocatorStack)
/* korl-clipboard interface ***************************************************/
typedef enum Korl_Clipboard_DataFormat
    { KORL_CLIPBOARD_DATA_FORMAT_UTF8// the size of the string returned from korl_clipboard_get _includes_ a null-terminator character
} Korl_Clipboard_DataFormat;
#define KORL_FUNCTION_korl_clipboard_set(name) void name(Korl_Clipboard_DataFormat dataFormat, acu8 data)
#define KORL_FUNCTION_korl_clipboard_get(name) acu8 name(Korl_Clipboard_DataFormat dataFormat, Korl_Memory_AllocatorHandle allocator)
/* korl-crash interface *******************************************************/
/** Do not call this function directly; use the \c korl_assert macro instead! */
#define KORL_FUNCTION__korl_crash_assertConditionFailed(name) void name(const wchar_t* conditionString, const wchar_t* cStringFileName\
                                                                       ,const wchar_t* cStringFunctionName, int lineNumber)
/* korl-stb-ds interface ******************************************************/
#define KORL_FUNCTION__korl_stb_ds_reallocate(name) void* name(void* context, void* allocation, u$ bytes, const wchar_t*const file, int line)
#define KORL_FUNCTION__korl_stb_ds_free(name)       void  name(void* context, void* allocation)
/* korl-string interface ******************************************************/
#define KORL_FUNCTION_korl_string_utf8_to_f32(name)             f32      name(acu8 utf8, u8** out_utf8F32End, bool* out_resultIsValid)
#define KORL_FUNCTION_korl_string_formatVaListUtf8(name)        char*    name(Korl_Memory_AllocatorHandle allocatorHandle, const char* format, va_list vaList)
#define KORL_FUNCTION_korl_string_formatVaListUtf16(name)       wchar_t* name(Korl_Memory_AllocatorHandle allocatorHandle, const wchar_t* format, va_list vaList)
#define KORL_FUNCTION_korl_string_formatBufferVaListUtf16(name) i$       name(wchar_t* buffer, u$ bufferBytes, const wchar_t* format, va_list vaList)
#define KORL_FUNCTION_korl_string_unicode_toUpper(name)         u32      name(u32 codePoint)
/* korl-algorithm interface ***************************************************/
#define KORL_ALGORITHM_COMPARE(name) int name(const void* a, const void* b)
typedef KORL_ALGORITHM_COMPARE(fnSig_korl_algorithm_compare);
#define KORL_ALGORITHM_COMPARE_CONTEXT(name) int name(const void* a, const void* b, void* context)
typedef KORL_ALGORITHM_COMPARE_CONTEXT(fnSig_korl_algorithm_compare_context);
#define KORL_FUNCTION_korl_algorithm_sort_quick(name)                     void  name(void* array, u$ arraySize, u$ arrayStride, fnSig_korl_algorithm_compare* compare)
#define KORL_FUNCTION_korl_algorithm_sort_quick_context(name)             void  name(void* array, u$ arraySize, u$ arrayStride, fnSig_korl_algorithm_compare_context* compare, void* context)
// #define KORL_FUNCTION_korl_algorithm_boundingVolumeHierarchy_create(name) void* name(Korl_Memory_AllocatorHandle allocator, void* array, u$ arraySize, u$ arrayStride)
/* FUNCTION TYPEDEFS **********************************************************/
#define _KORL_PLATFORM_API_MACRO_OPERATION(x) typedef KORL_FUNCTION_##x (fnSig_##x);
    #include "korl-interface-platform-api.h"
#undef _KORL_PLATFORM_API_MACRO_OPERATION
/* KORL API struct Definition *************************************************/
typedef struct KorlPlatformApi
{
    #define _KORL_PLATFORM_API_MACRO_OPERATION(x) fnSig_##x *x;
        #include "korl-interface-platform-api.h"
    #undef _KORL_PLATFORM_API_MACRO_OPERATION
} KorlPlatformApi;
/* KORL API function declarations *********************************************/
#ifdef KORL_PLATFORM
    /* in the platform's code module, we need to do standard forward-declaration of all KORL functions, 
        which is fine since the function definitions will be defined in the same translation unit */
    #define _KORL_PLATFORM_API_MACRO_OPERATION(x) korl_internal KORL_FUNCTION_##x(x);
        #include "korl-interface-platform-api.h"
    #undef _KORL_PLATFORM_API_MACRO_OPERATION
#else
    /* in the client code modules, we need to: 
        - declare all KORL functions as function pointers in the code module
        - declare & define (generate) a function which, when given a KorlPlatformApi, will populate all these function pointers with the correct address */
    #define _KORL_PLATFORM_API_MACRO_OPERATION(x) fnSig_##x *x;
        #include "korl-interface-platform-api.h"
    #undef _KORL_PLATFORM_API_MACRO_OPERATION
    korl_internal void korl_platform_getApi(KorlPlatformApi korlApi)
    {
        #define _KORL_PLATFORM_API_MACRO_OPERATION(x) (x) = korlApi.x;
            #include "korl-interface-platform-api.h"
        #undef _KORL_PLATFORM_API_MACRO_OPERATION
    }
#endif
