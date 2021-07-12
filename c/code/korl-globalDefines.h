#pragma once
#if defined(_MSC_VER) && !defined(__cplusplus)
    /* Windows SDK has a stupid bug in corecrt.h, and if we don't define this 
        here we get stupid warnings.  THANKS, MICROSHAFT!  SOURCE: 
        https://developercommunity.visualstudio.com/t/issue-in-corecrth-header-results-in-an-undefined-m/433021 */
    #define _CRT_HAS_CXX17 0
#endif// def _MSC_VER
/* C doesn't have bool by default until we include this I guess */
#include <stdbool.h>
/* for wchar_t */
#include <stddef.h>
/* support for stdalign.h, since MSVC just doesn't have this I guess */
#if defined(_MSC_VER)
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
typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;
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
/** calculate the size of an array 
 * (NOTE: does NOT work for dynamic arrays (only compile-time array sizes)!) */
#define korl_arraySize(array) (sizeof(array) / sizeof(array[0]))
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
#define KORL_C_CAST(type,variable) ((type)variable)
/* macro for automatically initializing stack variables to 0 */
#define KORL_ZERO_STACK(variableType, variableIdentifier) \
    variableType variableIdentifier;\
    memset(&(variableIdentifier), 0, sizeof(variableIdentifier));
