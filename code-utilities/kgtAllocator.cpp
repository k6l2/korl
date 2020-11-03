#include "kgtAllocator.h"
#include "kgtAllocatorGeneral.h"
#include "kgtAllocatorLinear.h"
union KgtAllocator
{
	KgtAllocatorType    type;
	KgtAllocatorLinear  linear;
	KgtAllocatorGeneral general;
};
internal KgtAllocatorHandle 
	kgtAllocInit(KgtAllocatorType type, void* memoryStart, size_t byteCount)
{
	KgtAllocatorHandle result = nullptr;
	switch(type)
	{
		case KgtAllocatorType::LINEAR:
			result = kgtAllocLinearInit(memoryStart, byteCount);
			break;
		case KgtAllocatorType::GENERAL:
			result = kgtAllocGeneralInit(memoryStart, byteCount);
			break;
		default:
			KLOG(ERROR, "Invalid allocator type (%i)", static_cast<u32>(type));
			break;
	}
	return result;
}
internal void kgtAllocReset(KgtAllocatorHandle hKal)
{
	KgtAllocator*const kAlloc = reinterpret_cast<KgtAllocator*>(hKal);
	switch(kAlloc->type)
	{
		case KgtAllocatorType::LINEAR:
			kgtAllocLinearReset(&kAlloc->linear);
			break;
		case KgtAllocatorType::GENERAL:
			korlAssert(!"NOT IMPLEMENTED");
			break;
		default:
			KLOG(ERROR, "Invalid allocator type (%i)", 
			     static_cast<u32>(kAlloc->type));
			break;
	}
}
internal void* kgtAllocAlloc(KgtAllocatorHandle hKal, size_t byteCount)
{
	KgtAllocator*const kAlloc = reinterpret_cast<KgtAllocator*>(hKal);
	switch(kAlloc->type)
	{
		case KgtAllocatorType::LINEAR:
			return kgtAllocLinearAlloc(&kAlloc->linear, byteCount);
		case KgtAllocatorType::GENERAL:
			return kgtAllocGeneralAlloc(&kAlloc->general, byteCount);
		default:
			KLOG(ERROR, "Invalid allocator type (%i)", 
			     static_cast<u32>(kAlloc->type));
			break;
	}
	return nullptr;
}
internal void* kgtAllocRealloc(
	KgtAllocatorHandle hKal, void* allocatedAddress, size_t newByteCount)
{
	KgtAllocator*const kAlloc = reinterpret_cast<KgtAllocator*>(hKal);
	switch(kAlloc->type)
	{
		case KgtAllocatorType::LINEAR:
			return kgtAllocLinearRealloc(
				&kAlloc->linear, allocatedAddress, newByteCount);
		case KgtAllocatorType::GENERAL:
			return kgtAllocGeneralRealloc(
				&kAlloc->general, allocatedAddress, newByteCount);
		default:
			KLOG(ERROR, "Invalid allocator type (%i)", 
			     static_cast<u32>(kAlloc->type));
			break;
	}
	return nullptr;
}
internal void kgtAllocFree(KgtAllocatorHandle hKal, void* allocatedAddress)
{
	KgtAllocator*const kAlloc = reinterpret_cast<KgtAllocator*>(hKal);
	switch(kAlloc->type)
	{
		case KgtAllocatorType::LINEAR:
			kgtAllocLinearFree(&kAlloc->linear, allocatedAddress);
			break;
		case KgtAllocatorType::GENERAL:
			kgtAllocGeneralFree(&kAlloc->general, allocatedAddress);
			break;
		default:
			KLOG(ERROR, "Invalid allocator type (%i)", 
			     static_cast<u32>(kAlloc->type));
			break;
	}
}
internal size_t kgtAllocUsedBytes(const KgtAllocatorHandle hKal)
{
	KgtAllocator*const kAlloc = reinterpret_cast<KgtAllocator*>(hKal);
	switch(kAlloc->type)
	{
		case KgtAllocatorType::LINEAR:
			return kgtAllocLinearUsedBytes(&kAlloc->linear);
		case KgtAllocatorType::GENERAL:
			return kgtAllocGeneralUsedBytes(&kAlloc->general);
		default:
			KLOG(ERROR, "Invalid allocator type (%i)", 
			     static_cast<u32>(kAlloc->type));
			break;
	}
	return 0;
}
internal size_t kgtAllocMaxTotalUsableBytes(const KgtAllocatorHandle hKal)
{
	KgtAllocator*const kAlloc = reinterpret_cast<KgtAllocator*>(hKal);
	switch(kAlloc->type)
	{
		case KgtAllocatorType::LINEAR:
			return kgtAllocLinearMaxTotalUsableBytes(&kAlloc->linear);
		case KgtAllocatorType::GENERAL:
			return kgtAllocGeneralMaxTotalUsableBytes(&kAlloc->general);
		default:
			KLOG(ERROR, "Invalid allocator type (%i)", 
			     static_cast<u32>(kAlloc->type));
			break;
	}
	return 0;
}
internal void kgtAllocUnitTest(const KgtAllocatorHandle hKal)
{
	void* allocs[16];
	allocs[0] = kgtAllocRealloc(hKal, 0, 4096);
	allocs[1] = kgtAllocAlloc(hKal, 16448);
	kgtAllocFree(hKal, allocs[0]);
	allocs[0] = kgtAllocAlloc(hKal, 16384);
	kgtAllocFree(hKal, allocs[1]);
	kgtAllocFree(hKal, allocs[0]);
	allocs[0] = kgtAllocRealloc(hKal, 0, 4096);
	allocs[1] = kgtAllocAlloc(hKal, 16416);
	allocs[2] = kgtAllocRealloc(hKal, 0, 4096);
	allocs[3] = kgtAllocAlloc(hKal, 16448);
	allocs[4] = kgtAllocRealloc(hKal, 0, 4096);
	allocs[5] = kgtAllocAlloc(hKal, 4128);
	kgtAllocFree(hKal, allocs[0]);
	kgtAllocFree(hKal, allocs[4]);
	allocs[0] = kgtAllocAlloc(hKal, 4096);
	allocs[4] = kgtAllocRealloc(hKal, 0, 4096);
	allocs[6] = kgtAllocAlloc(hKal, 1040);
	allocs[7] = kgtAllocRealloc(hKal, 0, 4096);
	allocs[8] = kgtAllocAlloc(hKal, 4128);
	allocs[9] = kgtAllocRealloc(hKal, 0, 4096);
	allocs[10] = kgtAllocAlloc(hKal, 4128);
	kgtAllocFree(hKal, allocs[5]);
	kgtAllocFree(hKal, allocs[9]);
	allocs[5] = kgtAllocAlloc(hKal, 4096);
	allocs[9] = kgtAllocAlloc(hKal, 16384);
	kgtAllocFree(hKal, allocs[7]);
	allocs[7] = kgtAllocAlloc(hKal, 4096);
	kgtAllocFree(hKal, allocs[2]);
	allocs[2] = kgtAllocAlloc(hKal, 16384);
	kgtAllocFree(hKal, allocs[4]);
	allocs[4] = kgtAllocAlloc(hKal, 1024);
	allocs[11] = kgtAllocRealloc(hKal, 0, 4096);
	allocs[12] = kgtAllocAlloc(hKal, 1040);
	kgtAllocFree(hKal, allocs[8]);
	kgtAllocFree(hKal, allocs[0]);
	kgtAllocFree(hKal, allocs[10]);
	kgtAllocFree(hKal, allocs[5]);
	kgtAllocFree(hKal, allocs[6]);
	kgtAllocFree(hKal, allocs[1]);
	kgtAllocFree(hKal, allocs[11]);
	allocs[0] = kgtAllocAlloc(hKal, 1024);
	kgtAllocFree(hKal, allocs[3]);
	kgtAllocFree(hKal, allocs[4]);
	kgtAllocFree(hKal, allocs[9]);
	kgtAllocFree(hKal, allocs[7]);
	kgtAllocFree(hKal, allocs[12]);
	kgtAllocFree(hKal, allocs[2]);
	kgtAllocFree(hKal, allocs[0]);
}
#include "kgtAllocatorGeneral.cpp"
#include "kgtAllocatorLinear.cpp"
