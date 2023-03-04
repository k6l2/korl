#pragma once
#include "korl-globalDefines.h"
// #define KORL_PLATFORM_MEMORY_FILL(name)    void name(void* memory, u$ bytes, u8 pattern)
#define KORL_FUNCTION_korl_memory_zero(name)    void name(void* memory, u$ bytes)
#define KORL_FUNCTION_korl_memory_copy(name)    void name(void* destination, const void* source, u$ bytes)
#define KORL_FUNCTION_korl_memory_move(name)    void name(void* destination, const void* source, u$ bytes)
/**  Should function in the same way as memcmp from the C standard library.
 * \return \c -1 if the first byte that differs has a lower value in \c a than 
 *    in \c b, \c 1 if the first byte that differs has a higher value in \c a 
 *    than in \c b, and \c 0 if the two memory blocks are equal */
#define KORL_FUNCTION_korl_memory_compare(name) int  name(const void* a, const void* b, size_t bytes)
/**
 * \return \c 0 if the length & contents of the arrays are equal
 */
#define KORL_FUNCTION_korl_memory_arrayU8Compare(name) int name(const u8* dataA, u$ sizeA, const u8* dataB, u$ sizeB)
/**
 * \return \c 0 if the length & contents of the arrays are equal
 */
#define KORL_FUNCTION_korl_memory_arrayU16Compare(name) int name(const u16* dataA, u$ sizeA, const u16* dataB, u$ sizeB)
#define KORL_FUNCTION_korl_memory_acu16_hash(name) u$ name(acu16 arrayCU16)
typedef u16 Korl_Memory_AllocatorHandle;
typedef enum Korl_Memory_AllocatorType
    { KORL_MEMORY_ALLOCATOR_TYPE_LINEAR
    , KORL_MEMORY_ALLOCATOR_TYPE_GENERAL
} Korl_Memory_AllocatorType;
typedef enum Korl_Memory_AllocatorFlags
    { KORL_MEMORY_ALLOCATOR_FLAGS_NONE                        = 0
    , KORL_MEMORY_ALLOCATOR_FLAG_DISABLE_THREAD_SAFETY_CHECKS = 1 << 0
    , KORL_MEMORY_ALLOCATOR_FLAG_EMPTY_EVERY_FRAME            = 1 << 1
    , KORL_MEMORY_ALLOCATOR_FLAG_SERIALIZE_SAVE_STATE         = 1 << 2
} Korl_Memory_AllocatorFlags;
typedef struct Korl_Heap_CreateInfo
{
    u$          initialHeapBytes;// ignored if heapDescriptorCount is non-zero
    u32         heapDescriptorCount;// if non-zero, all fields below _must_ be populated with valid data
    const void* heapDescriptors;
    u32         heapDescriptorStride;
    u32         heapDescriptorOffset_addressStart;
    u32         heapDescriptorOffset_addressEnd;
} Korl_Heap_CreateInfo;
typedef enum Korl_Memory_AllocationMeta_DefragmentState
    {KORL_MEMORY_ALLOCATION_META_DEFRAGMENT_STATE_UNKNOWN// defragment has not yet been called on the heap containing this allocation
    ,KORL_MEMORY_ALLOCATION_META_DEFRAGMENT_STATE_MANAGED// the user has properly passed this allocation as a DefragmentPointer to a call to defragment
    ,KORL_MEMORY_ALLOCATION_META_DEFRAGMENT_STATE_UNMANAGED// defragment has been called on the heap containing this allocation, but the user did _not_ add it to the DefragmentPointer manifest
} Korl_Memory_AllocationMeta_DefragmentState;
typedef struct Korl_Memory_AllocationMeta
{
    const wchar_t*                             file;
    int                                        line;
    /** The amount of actual memory used by the caller.  The grand total amount 
     * of memory used by an allocation will likely be the sum of the following:  
     * - the allocation meta data
     * - the actual memory used by the caller
     * - any additional padding required by the allocator (likely to round 
     *   everything up to the nearest page size) */
    u$                                         bytes;
    Korl_Memory_AllocationMeta_DefragmentState defragmentState;
} Korl_Memory_AllocationMeta;
#define KORL_HEAP_ENUMERATE_ALLOCATIONS_CALLBACK(name) bool name(void* userData, const void* allocation, const Korl_Memory_AllocationMeta* meta, u$ grossBytes, u$ netBytes)
#define KORL_HEAP_ENUMERATE_CALLBACK(name)             void name(void* userData, const void* virtualAddressStart, const void* virtualAddressEnd)
typedef KORL_HEAP_ENUMERATE_ALLOCATIONS_CALLBACK       (fnSig_korl_heap_enumerateAllocationsCallback);
typedef KORL_HEAP_ENUMERATE_CALLBACK                   (fnSig_korl_heap_enumerateCallback);
typedef struct Korl_Heap_DefragmentPointer Korl_Heap_DefragmentPointer;
#define KORL_HEAP_ON_ALLOCATION_MOVED_CALLBACK(name)   void name(void* userData, void* allocationAddress, u$ allocationBytes, i$ byteOffsetFromOldAddress, void* allocationAddressParent, Korl_Heap_DefragmentPointer* defragmentPointers, u$ defragmentPointersSize)
typedef KORL_HEAP_ON_ALLOCATION_MOVED_CALLBACK         (fnSig_korl_heap_onAllocationMovedCallback);
typedef struct Korl_Heap_DefragmentPointer
{
    void**                                     userAddressPointer;
    i32                                        userAddressByteOffset;// an offset applied to `*userAddressPointer` to determine the true allocation address of an opaque datatype pointer, as well as write the correct address after defragmentation takes place on that allocation; example usage: caller has a stb_ds array `Foo* stbDaFoos = NULL; mcarrsetcap(memoryContext, stbDaFoos, 8);`, so caller passes a `(Korl_Heap_DefragmentPointer){KORL_C_CAST(void**, &stbDaFoos), -KORL_C_CAST(i32, sizeof(stbds_array_header))}` to `korl_heap_*_defragment`
    void*                                      parent;// some allocations will themselves contain pointers to other allocations in the same heap; when a DefragmentPointer's userAddress is changed, all recursive children of it should be considered "stale"/dangling, since at that point the algorithm wont be able to update the pointer within the data struct that just moved if its child is also defragmented; the defragmentation algorithm needs to know this relationship so that it doesn't accidentally move a stale DefragmentPointer; also, note that this is not const, because a custom `onAllocationMoved` callback for a DefragmentPointer has the authority to modify the *parent as they see fit
    fnSig_korl_heap_onAllocationMovedCallback* onAllocationMovedCallback;// optional; if non-NULL, this function will be called the moment when `*userAddressPointer` changes during defragmentation; useful if you have a data structure that stores pointers, which are just addresses within the same allocation (offset from the allocation's root address)
    void*                                      onAllocationMovedCallbackUserData;// value passed to `onAllocationMovedCallback`; I _highly_ recommend passing memory from a stack allocator or some other such memory with an immutable address
} Korl_Heap_DefragmentPointer;
/** after testing the defragment API, I have found that it is _extremely_ 
 * annoying checking for valid pointers, filling out the DefragmentPointer 
 * struct, and pushing them onto the stb_ds array of DefragmentPointers; thus, 
 * I have created a set of convenience macros to mitigate all this overhead: */
#define KORL_MEMORY_STB_DA_DEFRAGMENT_CALLBACK(stackAllocatorHandle, stbDaDefragmentPointers, allocation, onAllocationMovedCallback) \
    if(allocation)\
    {\
        mcarrpush(KORL_STB_DS_MC_CAST(stackAllocatorHandle), (stbDaDefragmentPointers)\
                 ,(KORL_STRUCT_INITIALIZE(Korl_Heap_DefragmentPointer){KORL_C_CAST(void**, &(allocation)), 0, NULL, (onAllocationMovedCallback), NULL}));\
    }
#define KORL_MEMORY_STB_DA_DEFRAGMENT(stackAllocatorHandle, stbDaDefragmentPointers, allocation) \
    KORL_MEMORY_STB_DA_DEFRAGMENT_CALLBACK(stackAllocatorHandle, stbDaDefragmentPointers, allocation, NULL)
#define KORL_MEMORY_STB_DA_DEFRAGMENT_CHILD(stackAllocatorHandle, stbDaDefragmentPointers, allocation, allocationParent) \
    if(allocation)\
    {\
        mcarrpush(KORL_STB_DS_MC_CAST(stackAllocatorHandle), (stbDaDefragmentPointers)\
                 ,(KORL_STRUCT_INITIALIZE(Korl_Heap_DefragmentPointer){KORL_C_CAST(void**, &(allocation)), 0, allocationParent, NULL, NULL}));\
    }
#define KORL_MEMORY_STB_DA_DEFRAGMENT_STB_ARRAY_CHILD(stackAllocatorHandle, stbDaDefragmentPointers, stbDsArray, allocationParent) \
    if(stbDsArray)\
    {\
        mcarrpush(KORL_STB_DS_MC_CAST(stackAllocatorHandle), (stbDaDefragmentPointers)\
                 ,(KORL_STRUCT_INITIALIZE(Korl_Heap_DefragmentPointer){KORL_C_CAST(void**, &(stbDsArray)), -KORL_C_CAST(i32, sizeof(stbds_array_header)), allocationParent, NULL, NULL}));\
    }
#define KORL_MEMORY_STB_DA_DEFRAGMENT_STB_HASHMAP_CHILD(stbDaDefragmentPointersMemoryContext, stbDaDefragmentPointers, stbDsHashMap, allocationParent) \
    _korl_stb_ds_stbDaDefragment_hashMap(KORL_STB_DS_MC_CAST(stbDaDefragmentPointersMemoryContext), stbDaDefragmentPointers, KORL_C_CAST(void**, &(stbDsHashMap)), sizeof(*(stbDsHashMap)), allocationParent)
/** As of right now, the smallest possible value for \c maxBytes for a linear 
 * allocator is 16 kilobytes (4 pages), since two pages are required for the 
 * allocator struct, and a minimum of two pages are required for each allocation 
 * (one for the meta data page, and one for the actual allocation itself).  
 * This behavior is subject to change in the future. 
 * \param address [OPTIONAL] the allocator itself will attempt to be placed at 
 * the specified virtual address.  If this value is \c NULL , the operating 
 * system will choose this address for us. */
#define KORL_FUNCTION_korl_memory_allocator_create(name)               Korl_Memory_AllocatorHandle name(Korl_Memory_AllocatorType type, const wchar_t* allocatorName, Korl_Memory_AllocatorFlags flags, const Korl_Heap_CreateInfo* heapCreateInfo)
/** \param address [OPTIONAL] the allocation will be placed at this exact 
 * address in the allocator.  Safety checks are performed to ensure that the 
 * specified address is valid (within allocator address range, not overlapping 
 * other allocations).  If this value is \c NULL , the allocator will choose the 
 * address of the allocation automatically (as you would typically expect). */
#define KORL_FUNCTION_korl_memory_allocator_allocate(name)             void*                       name(Korl_Memory_AllocatorHandle handle, u$ bytes, const wchar_t* file, int line, void* address)
#define KORL_FUNCTION_korl_memory_allocator_reallocate(name)           void*                       name(Korl_Memory_AllocatorHandle handle, void* allocation, u$ bytes, const wchar_t* file, int line)
#define KORL_FUNCTION_korl_memory_allocator_free(name)                 void                        name(Korl_Memory_AllocatorHandle handle, void* allocation, const wchar_t* file, int line)
#define KORL_FUNCTION_korl_memory_allocator_empty(name)                void                        name(Korl_Memory_AllocatorHandle handle)
#define KORL_FUNCTION_korl_memory_allocator_defragment(name)           void                        name(Korl_Memory_AllocatorHandle handle, Korl_Heap_DefragmentPointer* defragmentPointers, u$ defragmentPointersSize, Korl_Memory_AllocatorHandle handleStack)
#define KORL_FUNCTION_korl_memory_allocator_enumerateAllocators(name)  void                        name(fnSig_korl_memory_allocator_enumerateAllocatorsCallback* callback, void* callbackUserData)
#define KORL_FUNCTION_korl_memory_allocator_enumerateAllocations(name) void                        name(void* opaqueAllocator, fnSig_korl_heap_enumerateAllocationsCallback* callback, void* callbackUserData)
#define KORL_FUNCTION_korl_memory_allocator_enumerateHeaps(name)       void                        name(void* opaqueAllocator, fnSig_korl_heap_enumerateCallback* callback, void* callbackUserData)
