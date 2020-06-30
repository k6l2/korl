#pragma once
#include "global-defines.h"
using KalHandle = void*;
internal KalHandle kalInit(void* allocatorMemoryStart, 
                           size_t allocatorByteCount);
internal void* kalAlloc(KalHandle hKal, size_t allocationByteCount);
internal void* kalRealloc(KalHandle hKal, void* allocatedAddress, 
                          size_t newAllocationSize);
internal void kalReset(KalHandle hKal);
internal size_t kalUsedBytes(KalHandle hKal);
internal size_t kalMaxTotalUsableBytes(KalHandle hKal);