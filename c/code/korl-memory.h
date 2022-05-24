#pragma once
#include "korl-globalDefines.h"
#include "korl-interface-platform.h"
korl_internal void korl_memory_initialize(void);
korl_internal u$   korl_memory_pageBytes(void);
/**  Should function in the same way as memcmp from the C standard library.
 * \return \c -1 if the first byte that differs has a lower value in \c a than 
 *    in \c b, \c 1 if the first byte that differs has a higher value in \c a 
 *    than in \c b, and \c 0 if the two memory blocks are equal
 */
korl_internal int korl_memory_compare(const void* a, const void* b, size_t bytes);
/**
 * \return \c 0 if the two strings are equal
 */
korl_internal int korl_memory_stringCompare(const wchar_t* a, const wchar_t* b);
/**
 * \return the size of string \c s EXCLUDING the null terminator
 */
korl_internal u$ korl_memory_stringSize(const wchar_t* s);
/**
 * \return the number of characters copied from \c source into \c destination , 
 * INCLUDING the null terminator.  If the \c source cannot be copied into the 
 * \c destination then the size of \c source INCLUDING the null terminator and 
 * multiplied by -1 is returned, and \c destination is filled with the maximum 
 * number of characters that can be copied, including a null-terminator.
 */
korl_internal i$ korl_memory_stringCopy(const wchar_t* source, wchar_t* destination, u$ destinationSize);
korl_internal KORL_PLATFORM_MEMORY_ZERO(korl_memory_zero);
korl_internal KORL_PLATFORM_MEMORY_COPY(korl_memory_copy);
korl_internal KORL_PLATFORM_MEMORY_MOVE(korl_memory_move);
korl_internal bool korl_memory_isNull(const void* p, size_t bytes);
korl_internal wchar_t* korl_memory_stringFormat(Korl_Memory_AllocatorHandle allocatorHandle, const wchar_t* format, ...);
korl_internal wchar_t* korl_memory_stringFormatVaList(Korl_Memory_AllocatorHandle allocatorHandle, const wchar_t* format, va_list vaList);
/**
 * \return the number of characters copied from \c format into \c buffer , 
 * INCLUDING the null terminator.  If the \c format cannot be copied into the 
 * \c buffer then the size of \c format , INCLUDING the null terminator, 
 * multiplied by -1, is returned.
 */
korl_internal i$ korl_memory_stringFormatBuffer(wchar_t* buffer, u$ bufferBytes, const wchar_t* format, ...);
korl_internal i$ korl_memory_stringFormatBufferVaList(wchar_t* buffer, u$ bufferBytes, const wchar_t* format, va_list vaList);
korl_internal KORL_PLATFORM_MEMORY_CREATE_ALLOCATOR    (korl_memory_allocator_create);
korl_internal KORL_PLATFORM_MEMORY_ALLOCATOR_ALLOCATE  (korl_memory_allocator_allocate);
korl_internal KORL_PLATFORM_MEMORY_ALLOCATOR_REALLOCATE(korl_memory_allocator_reallocate);
korl_internal KORL_PLATFORM_MEMORY_ALLOCATOR_FREE      (korl_memory_allocator_free);
korl_internal KORL_PLATFORM_MEMORY_ALLOCATOR_EMPTY     (korl_memory_allocator_empty);
korl_internal void korl_memory_allocator_emptyStackAllocators(void);
/** \param allocatorHandle the allocator to use to allocate memory to store the report 
 * \return the address of the allocated memory report */
korl_internal void* korl_memory_reportGenerate(void);
/** \param reportAddress the address of the return value of a previous call to \c korl_memory_reportGenerate */
korl_internal void korl_memory_reportLog(void* reportAddress);
