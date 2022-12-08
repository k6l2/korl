#pragma once
#if defined(KORL_PLATFORM_WINDOWS) && !defined(__cplusplus)
    /* Windows SDK has a stupid bug in corecrt.h, and if we don't define this 
        here we get stupid warnings.  THANKS, MICROSHAFT!  SOURCE: 
        https://developercommunity.visualstudio.com/t/issue-in-corecrth-header-results-in-an-undefined-m/433021 */
    #define _CRT_HAS_CXX17 0
#endif// def KORL_PLATFORM_WINDOWS
/* C doesn't have bool by default until we include this I guess */
#include <stdbool.h>
/* for wchar_t */
#include <stddef.h>// note that this pulls in <corecrt.h>!!!
/* support for stdalign.h, since MSVC just doesn't have this I guess */
#if defined(_MSC_VER) && !defined(__cplusplus)
    #define alignof _Alignof
    #define alignas(byteCount) __declspec(align(byteCount))
#endif// defined(_MSC_VER)
/** disambiguations of the \c static key word to improve project 
 * searchability */
#define korl_internal        static
#define korl_global_variable static
#define korl_global_const    static const
#define korl_shared_variable static
#define korl_shared_const    static const
/** simple primitive type definitions */
typedef char      i8;
typedef short     i16;
typedef int       i32;
typedef long long i64;
typedef long long i$;
typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;
typedef unsigned long long u$;
typedef float  f32;
typedef double f64;
// and if any of these asserts fail, we can just fix these based on target 
//  platform architecture/compiler:
_STATIC_ASSERT(sizeof(i8 ) == 1);
_STATIC_ASSERT(sizeof(i16) == 2);
_STATIC_ASSERT(sizeof(i32) == 4);
_STATIC_ASSERT(sizeof(i64) == 8);
_STATIC_ASSERT(sizeof(u8 ) == 1);
_STATIC_ASSERT(sizeof(u16) == 2);
_STATIC_ASSERT(sizeof(u32) == 4);
_STATIC_ASSERT(sizeof(u64) == 8);
_STATIC_ASSERT(sizeof(f32) == 4);
_STATIC_ASSERT(sizeof(f64) == 8);
/* definitions for useful values of these primitive types */
#define KORL_U8_MAX  0xFF
#define KORL_U16_MAX 0xFFFF
#define KORL_U32_MAX 0xFFFFFFFF
#define KORL_U64_MAX 0xFFFFFFFFFFFFFFFF
#define KORL_I8_MAX  0x7F
#define KORL_I16_MAX 0x7FFF
#define KORL_I32_MAX 0x7FFFFFFF
#define KORL_I64_MAX 0x7FFFFFFFFFFFFFFF
#define KORL_I8_MIN  ~KORL_I8_MAX
#define KORL_I16_MIN ~KORL_I16_MAX
#define KORL_I32_MIN ~KORL_I32_MAX
#define KORL_I64_MIN ~KORL_I64_MAX
#define KORL_F32_MIN      (-3.402823466e+38F)
#define KORL_F32_SMALLEST ( 1.175494351e-38F)
#define KORL_F32_MAX      ( 3.402823466e+38F)
/** String primitive datatypes.  CRT strings (null-terminated arrays of 
 * characters) are very outdated, and their performance benefits are dubious, 
 * while the number of instabilities they introduce in the code seems 
 * disproportionately higher.  I would much rather prefer to pass raw strings 
 * that contain explicit sizes, instead of simple raw string pointers that 
 * possess an implicit size (defined by the null-terminator character) just for 
 * the safety benefits alone.  Perhaps if I ever decide to remove the CRT 
 * completely, we can just use these everywhere instead of char & wchar. */
typedef struct ai8
{
    u$  size;
    i8* data;
} ai8;
typedef struct au8
{
    u$  size;
    u8* data;
} au8;
typedef struct au16
{
    u$   size;
    u16* data;
} au16;
typedef struct aci8
{
    u$        size;
    const i8* data;
} aci8;
typedef struct acu8
{
    u$        size;
    const u8* data;
} acu8;
typedef struct acu16
{
    u$         size;
    const u16* data;
} acu16;
#define KORL_RAW_CONST_UTF8(x) KORL_STRUCT_INITIALIZE(acu8){.size=korl_memory_stringSizeUtf8(x), .data=KORL_C_CAST(const u8*,(x))}
/** calculate the size of an array 
 * (NOTE: does NOT work for dynamic arrays (only compile-time array sizes)!) */
#define korl_arraySize(array) (sizeof(array) / sizeof((array)[0]))
/** C macro trick to automatically determine how many variadic arguments were 
 * passed to a macro.  ASSUMPTION: we are never going to pass more than 70 
 * arguments to a macro!  SOURCE: https://stackoverflow.com/a/36015150 */
#if defined(_MSC_VER)
#define KORL_GET_ARG_COUNT(...) \
    KORL_INTERNAL_EXPAND_ARGS_PRIVATE(KORL_INTERNAL_ARGS_AUGMENTER(__VA_ARGS__))
#define KORL_INTERNAL_ARGS_AUGMENTER(...) unused, __VA_ARGS__
#define KORL_INTERNAL_EXPAND(x) x
#define KORL_INTERNAL_EXPAND_ARGS_PRIVATE(...) \
    KORL_INTERNAL_EXPAND(KORL_INTERNAL_GET_ARG_COUNT_PRIVATE(__VA_ARGS__, \
    69, 68, 67, 66, 65, \
    64, 63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, \
    48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, \
    32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, \
    16, 15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1, 0))
#define KORL_INTERNAL_GET_ARG_COUNT_PRIVATE(\
    _1_, _2_, _3_, _4_, _5_, _6_, _7_, _8_, _9_, _10, \
    _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, \
    _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, \
    _31, _32, _33, _34, _35, _36, _37, _38, _39, _40, \
    _41, _42, _43, _44, _45, _46, _47, _48, _49, _50, \
    _51, _52, _53, _54, _55, _56, _57, _58, _59, _60, \
    _61, _62, _63, _64, _65, _66, _67, _68, _69, _70, count, ...) count
#else /* this technique does not work w/ MSVC for 0 arguments :( */
#define KORL_GET_ARG_COUNT(...) \
    KORL_INTERNAL_GET_ARG_COUNT_PRIVATE(0, ## __VA_ARGS__, \
        70, 69, 68, 67, 66, 65, \
        64, 63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, \
        48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, \
        32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, \
        16, 15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1, 0)
#define KORL_INTERNAL_GET_ARG_COUNT_PRIVATE(_0_, \
    _1_, _2_, _3_, _4_, _5_, _6_, _7_, _8_, _9_, _10, \
    _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, \
    _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, \
    _31, _32, _33, _34, _35, _36, _37, _38, _39, _40, \
    _41, _42, _43, _44, _45, _46, _47, _48, _49, _50, \
    _51, _52, _53, _54, _55, _56, _57, _58, _59, _60, \
    _61, _62, _63, _64, _65, _66, _67, _68, _69, _70, count, ...) count
#endif//0
/* macro for c-style casts to improve project searchability */
#define KORL_C_CAST(type,variable) ((type)(variable))
/* macro for C/C++ compatible struct initializer lists */
#ifdef __cplusplus
    #define KORL_STRUCT_INITIALIZE(type) type
#else
    #define KORL_STRUCT_INITIALIZE(type) (type)
#endif
#ifdef __cplusplus
    #define KORL_STRUCT_INITIALIZE_ZERO(type) type{}
#else
    #define KORL_STRUCT_INITIALIZE_ZERO(type) (type){0}
#endif
/* convenience macros specifically for korl-memory module, which automatically 
    inject file/line information */
#define korl_allocate(handle, bytes)               korl_memory_allocator_allocate  (handle,             bytes, __FILEW__, __LINE__, NULL)
#define korl_reallocate(handle, allocation, bytes) korl_memory_allocator_reallocate(handle, allocation, bytes, __FILEW__, __LINE__)
#define korl_free(handle, allocation)              korl_memory_allocator_free      (handle, allocation,        __FILEW__, __LINE__)
/* macro for automatically initializing stack variables to 0 */
#define KORL_ZERO_STACK(variableType, variableIdentifier) \
    variableType variableIdentifier;\
    korl_memory_zero(&(variableIdentifier), sizeof(variableIdentifier));
/* same as KORL_ZERO_STACK, except for arrays */
#define KORL_ZERO_STACK_ARRAY(variableType, variableIdentifier, arraySize) \
    variableType variableIdentifier[arraySize]; \
    korl_memory_zero(variableIdentifier, sizeof(variableIdentifier));
/* Having the game manage its own copy of these macros means we don't have to 
    include platform-specific KORL modules! */
#define korl_assert(condition) \
    ((condition) ? (void)0 : _korl_crash_assertConditionFailed(L""#condition, __FILEW__, __FUNCTIONW__, __LINE__))
/** Note that `logLevel` must be passed as the final word of the associated 
 * identifier in the `KorlEnumLogLevel` enumeration.
 * example usage: 
 *  korl_shared_const wchar_t*const USER_NAME = L"Kyle";
 *  korl_log(INFO, "hey there, %ws!", USER_NAME); */
#define korl_log(logLevel, format, ...) \
    _korl_log_variadic(KORL_GET_ARG_COUNT(__VA_ARGS__), KORL_LOG_LEVEL_##logLevel\
                      ,__FILEW__, __FUNCTIONW__, __LINE__, L ## format, ##__VA_ARGS__)
#define korl_log_noMeta(logLevel, format, ...) \
    _korl_log_variadic(KORL_GET_ARG_COUNT(__VA_ARGS__), KORL_LOG_LEVEL_##logLevel\
                      ,NULL, NULL, -1, L ## format, ##__VA_ARGS__)
/* Allow the pre-processor to store compiler definitions as string literals
    Example: TCHAR APPLICATION_NAME[] = L""KORL_DEFINE_TO_CSTR(KORL_APP_NAME);
    Source: https://stackoverflow.com/a/39108392 */
#define KORL_STRINGIFY(define) #define
#define KORL_DEFINE_TO_CSTR(d) KORL_STRINGIFY(d)
/* This is a really stupid hack to force the export of struct symbols when there 
    is no declared instances of the struct in the project.  If I can figure out 
    a way to force the linker to export struct symbols for structs that have no 
    variables defined (outside of internal code or implicitly via reinterpret 
    casting of memory), then this should be deleted probably.  Another option to 
    achieve this would be to use `__declspec( dllexport )` on MSVC for example, 
    but I'm hoping this method is fully portable.
    Example usage:
        struct TestStruct
        {
            // ... some data declared in here
        } FORCE_SYMBOL_EXPORT; */
#define CONCAT_(x,y)    x##y
#define CONCAT(x,y)     CONCAT_(x,y)
#define FORCE_SYMBOL_EXPORT \
    CONCAT(_hack_force_symbol_export_DO_NOT_USE_, __COUNTER__)
