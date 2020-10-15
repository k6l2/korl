#pragma once
#include "kutil.h"
using KgtAllocatorHandle = void*;
enum class KgtAllocatorType : u8
	{ LINEAR
	, GENERAL };
internal KgtAllocatorHandle 
	kgtAllocInit(KgtAllocatorType type, void* memoryStart, size_t byteCount);
internal void 
	kgtAllocReset(KgtAllocatorHandle hKal);
internal void* 
	kgtAllocAlloc(KgtAllocatorHandle hKal, size_t byteCount);
internal void* 
	kgtAllocRealloc(
		KgtAllocatorHandle hKal, void* allocatedAddress, size_t newByteCount);
internal void 
	kgtAllocFree(KgtAllocatorHandle hKal, void* allocatedAddress);
internal size_t 
	kgtAllocUsedBytes(const KgtAllocatorHandle hKal);
internal size_t 
	kgtAllocMaxTotalUsableBytes(const KgtAllocatorHandle hKal);
internal void 
	kgtAllocUnitTest(const KgtAllocatorHandle hKal);
