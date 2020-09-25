#pragma once
#include "kutil.h"
using KAllocatorHandle = void*;
enum class KAllocatorType : u8
	{ LINEAR
	, GENERAL };
internal KAllocatorHandle kAllocInit(
	KAllocatorType type, void* memoryStart, size_t byteCount);
internal void kAllocReset(KAllocatorHandle hKal);
internal void* kAllocAlloc(KAllocatorHandle hKal, size_t byteCount);
internal void* kAllocRealloc(
	KAllocatorHandle hKal, void* allocatedAddress, size_t newByteCount);
internal void kAllocFree(KAllocatorHandle hKal, void* allocatedAddress);
internal size_t kAllocUsedBytes(const KAllocatorHandle hKal);
internal size_t kAllocMaxTotalUsableBytes(const KAllocatorHandle hKal);

