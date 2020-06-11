#pragma once
#include "global-defines.h"
/** Kyle's General Allocator */
using KgaHandle = void*;
internal KgaHandle kgaInit(void* allocatorMemoryLocation, 
                           size_t allocatorByteCount);
internal void* kgaAlloc(KgaHandle kgaHandle, size_t allocationByteCount);
internal void* kgaRealloc(KgaHandle kgaHandle, void* allocatedAddress, 
                          size_t newAllocationSize);
internal void kgaFree(KgaHandle kgaHandle, void* allocatedAddress);
internal size_t kgaUsedBytes(KgaHandle kgaHandle);
internal size_t kgaMaxTotalUsableBytes(KgaHandle kgaHandle);