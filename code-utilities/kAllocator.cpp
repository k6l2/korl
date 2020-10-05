#include "kAllocator.h"
#include "kGeneralAllocator.h"
#include "kAllocatorLinear.h"
union KAllocator
{
	KAllocatorType type;
	KAllocatorLinear linear;
	KGeneralAllocator general;
};
internal KAllocatorHandle kAllocInit(
	KAllocatorType type, void* memoryStart, size_t byteCount)
{
	KAllocatorHandle result = nullptr;
	switch(type)
	{
		case KAllocatorType::LINEAR:
			result = kalInit(memoryStart, byteCount);
			break;
		case KAllocatorType::GENERAL:
			result = kgaInit(memoryStart, byteCount);
			break;
		default:
			KLOG(ERROR, "Invalid allocator type (%i)", static_cast<u32>(type));
			break;
	}
	return result;
}
internal void kAllocReset(KAllocatorHandle hKal)
{
	KAllocator*const kAlloc = reinterpret_cast<KAllocator*>(hKal);
	switch(kAlloc->type)
	{
		case KAllocatorType::LINEAR:
			kalReset(&kAlloc->linear);
			break;
		case KAllocatorType::GENERAL:
			kassert(!"NOT IMPLEMENTED");
			break;
		default:
			KLOG(ERROR, "Invalid allocator type (%i)", 
			     static_cast<u32>(kAlloc->type));
			break;
	}
}
internal void* kAllocAlloc(KAllocatorHandle hKal, size_t byteCount)
{
	KAllocator*const kAlloc = reinterpret_cast<KAllocator*>(hKal);
	switch(kAlloc->type)
	{
		case KAllocatorType::LINEAR:
			return kalAlloc(&kAlloc->linear, byteCount);
		case KAllocatorType::GENERAL:
			return kgaAlloc(&kAlloc->general, byteCount);
		default:
			KLOG(ERROR, "Invalid allocator type (%i)", 
			     static_cast<u32>(kAlloc->type));
			break;
	}
	return nullptr;
}
internal void* kAllocRealloc(
	KAllocatorHandle hKal, void* allocatedAddress, size_t newByteCount)
{
	KAllocator*const kAlloc = reinterpret_cast<KAllocator*>(hKal);
	switch(kAlloc->type)
	{
		case KAllocatorType::LINEAR:
			return kalRealloc(&kAlloc->linear, allocatedAddress, newByteCount);
		case KAllocatorType::GENERAL:
			return kgaRealloc(&kAlloc->general, allocatedAddress, newByteCount);
		default:
			KLOG(ERROR, "Invalid allocator type (%i)", 
			     static_cast<u32>(kAlloc->type));
			break;
	}
	return nullptr;
}
internal void kAllocFree(KAllocatorHandle hKal, void* allocatedAddress)
{
	KAllocator*const kAlloc = reinterpret_cast<KAllocator*>(hKal);
	switch(kAlloc->type)
	{
		case KAllocatorType::LINEAR:
			kalFree(&kAlloc->linear, allocatedAddress);
			break;
		case KAllocatorType::GENERAL:
			kgaFree(&kAlloc->general, allocatedAddress);
			break;
		default:
			KLOG(ERROR, "Invalid allocator type (%i)", 
			     static_cast<u32>(kAlloc->type));
			break;
	}
}
internal size_t kAllocUsedBytes(const KAllocatorHandle hKal)
{
	KAllocator*const kAlloc = reinterpret_cast<KAllocator*>(hKal);
	switch(kAlloc->type)
	{
		case KAllocatorType::LINEAR:
			return kalUsedBytes(&kAlloc->linear);
		case KAllocatorType::GENERAL:
			return kgaUsedBytes(&kAlloc->general);
		default:
			KLOG(ERROR, "Invalid allocator type (%i)", 
			     static_cast<u32>(kAlloc->type));
			break;
	}
	return 0;
}
internal size_t kAllocMaxTotalUsableBytes(const KAllocatorHandle hKal)
{
	KAllocator*const kAlloc = reinterpret_cast<KAllocator*>(hKal);
	switch(kAlloc->type)
	{
		case KAllocatorType::LINEAR:
			return kalMaxTotalUsableBytes(&kAlloc->linear);
		case KAllocatorType::GENERAL:
			return kgaMaxTotalUsableBytes(&kAlloc->general);
		default:
			KLOG(ERROR, "Invalid allocator type (%i)", 
			     static_cast<u32>(kAlloc->type));
			break;
	}
	return 0;
}
internal void kAllocUnitTest(const KAllocatorHandle hKal)
{
	void* allocs[16];
	allocs[0] = kAllocRealloc(hKal, 0, 4096);
	allocs[1] = kAllocAlloc(hKal, 16448);
	kAllocFree(hKal, allocs[0]);
	allocs[0] = kAllocAlloc(hKal, 16384);
	kAllocFree(hKal, allocs[1]);
	kAllocFree(hKal, allocs[0]);
	allocs[0] = kAllocRealloc(hKal, 0, 4096);
	allocs[1] = kAllocAlloc(hKal, 16416);
	allocs[2] = kAllocRealloc(hKal, 0, 4096);
	allocs[3] = kAllocAlloc(hKal, 16448);
	allocs[4] = kAllocRealloc(hKal, 0, 4096);
	allocs[5] = kAllocAlloc(hKal, 4128);
	kAllocFree(hKal, allocs[0]);
	kAllocFree(hKal, allocs[4]);
	allocs[0] = kAllocAlloc(hKal, 4096);
	allocs[4] = kAllocRealloc(hKal, 0, 4096);
	allocs[6] = kAllocAlloc(hKal, 1040);
	allocs[7] = kAllocRealloc(hKal, 0, 4096);
	allocs[8] = kAllocAlloc(hKal, 4128);
	allocs[9] = kAllocRealloc(hKal, 0, 4096);
	allocs[10] = kAllocAlloc(hKal, 4128);
	kAllocFree(hKal, allocs[5]);
	kAllocFree(hKal, allocs[9]);
	allocs[5] = kAllocAlloc(hKal, 4096);
	allocs[9] = kAllocAlloc(hKal, 16384);
	kAllocFree(hKal, allocs[7]);
	allocs[7] = kAllocAlloc(hKal, 4096);
	kAllocFree(hKal, allocs[2]);
	allocs[2] = kAllocAlloc(hKal, 16384);
	kAllocFree(hKal, allocs[4]);
	allocs[4] = kAllocAlloc(hKal, 1024);
	allocs[11] = kAllocRealloc(hKal, 0, 4096);
	allocs[12] = kAllocAlloc(hKal, 1040);
	kAllocFree(hKal, allocs[8]);
	kAllocFree(hKal, allocs[0]);
	kAllocFree(hKal, allocs[10]);
	kAllocFree(hKal, allocs[5]);
	kAllocFree(hKal, allocs[6]);
	kAllocFree(hKal, allocs[1]);
	kAllocFree(hKal, allocs[11]);
	allocs[0] = kAllocAlloc(hKal, 1024);
	kAllocFree(hKal, allocs[3]);
	kAllocFree(hKal, allocs[4]);
	kAllocFree(hKal, allocs[9]);
	kAllocFree(hKal, allocs[7]);
	kAllocFree(hKal, allocs[12]);
	kAllocFree(hKal, allocs[2]);
	kAllocFree(hKal, allocs[0]);
}
#include "kGeneralAllocator.cpp"
#include "kAllocatorLinear.cpp"
