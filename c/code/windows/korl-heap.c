#include "korl-heap.h"
#include "korl-windows-globalDefines.h"
#include "korl-interface-platform.h"// for KorlEnumLogLevel
#include "korl-memory.h"
#include "korl-memoryPool.h"
#include "korl-stb-ds.h"
#include "korl-checkCast.h"
#define _KORL_HEAP_BYTE_PATTERN_SENTINEL  0x5A
#define _KORL_HEAP_BYTE_PATTERN_FREE      0xA5
#define _KORL_HEAP_SENTINEL_PADDING_BYTES 64
#define _KORL_HEAP_GENERAL_USE_OLD 1//@TODO: remove this define & all unused code once rewrite is complete
#if _KORL_HEAP_GENERAL_USE_OLD
    /** A note about \c _KORL_HEAP_*_GUARD_* defines: after analyzing performance a 
     * bit with more realistic work loads, I've determined that this behavior 
     * (guarding unused pages) is just too aggressive for most scenarios.  We 
     * probably only want to do this when we are getting a memory crash somewhere & 
     * we need to perform extra validation steps (such as the code when 
     * _KORL_HEAP_GENERAL_DEBUG==1).  Ergo, we will just default this behavior to 
     * OFF until the time comes when we actually need it for testing/validation. */
    #define _KORL_HEAP_INVALID_BYTE_PATTERN 0xFE
    #define _KORL_HEAP_GENERAL_GUARD_UNUSED_ALLOCATION_PAGES 0
    #define _KORL_HEAP_GENERAL_GUARD_ALLOCATOR 0
    #define _KORL_HEAP_GENERAL_LAZY_COMMIT_PAGES 1
    #define _KORL_HEAP_GENERAL_DEBUG 0
#endif
#if _KORL_HEAP_GENERAL_USE_OLD
    korl_global_const i8 _KORL_HEAP_GENERAL_ALLOCATION_META_SEPARATOR[] = "[KORL-general-allocation]";
#endif
#if _KORL_HEAP_GENERAL_USE_OLD
/** Allocator properties: 
 * - all allocations are page-aligned
 * - all allocations are separated by an un-committed "guard page"
 * - when new allocations are created, we will start a search from the end of 
 *   the last allocation for available pages, which wraps around to the 
 *   beginning of the allocator's available pages if necessary (prioritize fast 
 *   allocations; assume that by the time we wrap around to the beginning of the 
 *   allocator's available pages, those pages will already be freed/available 
 *   again)
 * - if we end up suffering from memory fragmentation, we can always implement 
 *   a defragmentation routine and run it occasionally
 * Because of this, a good data structure to use to determine where to place new 
 * allocations would be a giant array of flags.  The status of each flag 
 * determines the availability of each page in the allocator.  Because bitfields 
 * are so compact, performing a search for the next available space will 
 * generally have very few cache misses.  Some back of the envelope 
 * calculations:
 * - page size on Windows is 4096 bytes
 * - this stores a maximum of 4096*8 = 32768 bits/flags
 * - if each flag indicates the status of a single page, this means with just a 
 *   single page of flags, we can store the status of 
 *   32768*4096 = 134217728 bytes of virtual memory
 * - assuming we must separate each allocation with a guard page, we still 
 *   maintain a maximum of 4096/2 = 2048 allocations
 * - since the entire allocator index is in a single page, and cache line size 
 *   is usually around 64 bytes, we only have to visit a maximum of 
 *   4096/64 = 64 cache lines to perform the search for an available set of 
 *   virtual memory addresses for the new allocation (and similarly, we retain 
 *   the ability to enumerate over all allocations relatively quickly, which is 
 *   a requirement of KORL allocators, mainly for the savestate feature; 
 *   enumeration of all allocations can be done easily because we know an 
 *   allocation is there if it is preceded by an unused guard page, or bit 
 *   pattern: 01*)
 * So with just a single page dedicated to allocator book-keeping, we can 
 * basically handle the vast majority of allocator cases.
 */
typedef struct _Korl_Heap_General
{
    _Korl_Heap_General* next;// support for "limitless" heap; if this allocator fails to allocate, we shunt to the `next` heap, or create a new heap if `next` doesn't exist yet
    u$ lastPageFlagRegisterIndex;
    // u$ bytes;//KORL-ISSUE-000-000-030: redundant: use QueryVirtualMemoryInformation instead
    u$ allocationPages;// this is the # which determines how many flags are actually relevant in the availablePageFlags member
    u$ allocationPagesCommitted;// in an attempt to reduce sys calls necessary for allocations; instead of decommitting & re-committing pages each time we allocate & free, using this variable we will just keep a full range of pages committed which we will "free" by zeroing the memory & guarding the page for later reallocation use
    u$ availablePageFlagsSize;// just used to determine the total size of the general allocator data structure
    u$ availablePageFlags[1];// see comments about this data structure for details of how this is used; the size of this array is determined by `bytes`, which should be calculated as `nextHighestDivision(nextHighestDivision(bytes, pageBytes), bitCount(u$))`
} _Korl_Heap_General;
typedef struct _Korl_Heap_General_AllocationMeta
{
    Korl_Memory_AllocationMeta allocationMeta;
    u$ pagesCommitted;// Overengineering; all this can do is allow us to support reserving of arbitrary # of pages.  In reality, I probably will never implement this, and if so, the page count can be derived from calculating `nextHighestDivision(sizeof(_Korl_Heap_General_AllocationMeta) + metaData.bytes, pageBytes)`
} _Korl_Heap_General_AllocationMeta;
#else
typedef struct _Korl_Heap_General
{
    _Korl_Heap_General* next;// support for "limitless" heap; if this heap fails to allocate, we shunt to the `next` heap, or create a new heap if `next` doesn't exist yet
} _Korl_Heap_General;
typedef struct _Korl_Heap_General_AllocationMeta
{
    Korl_Memory_AllocationMeta allocationMeta;
} _Korl_Heap_General_AllocationMeta;
#endif
/** it is likely that this struct is _not_ aligned to a page boundary, as we will 
 * likely place a sentinel between the page boundary & the struct */
typedef struct _Korl_Heap_Linear
{
    u$                 virtualPages;// _total_ # of virtual pages occupied by this heap, including the page(s) occupied by this struct & all allocations
    u$                 committedPages;// will always be <= virtualPages
    u$                 allocatedBytes;// gross-byte total of all allocations after (this heap struct + padding)
    bool               isFragmented;
    _Korl_Heap_Linear* next;// support for "limitless" heap; if this heap fails to allocate, we shunt to the `next` heap, or create a new heap if `next` doesn't exist yet
} _Korl_Heap_Linear;
/** each allocation in _Korl_Heap_Linear is in the form: 
 *   [_Korl_Heap_Linear_AllocationMeta][SENTINEL-meta][allocation][SENTINEL-allocation][unused]
 * where sizeof([unused]) is potentially == 0
 * - "gross bytes" is the sum of _all_ components
 * - an allocation is considered "free" if `meta.allocationMeta.bytes` <= 0 */
typedef struct _Korl_Heap_Linear_AllocationMeta
{
    Korl_Memory_AllocationMeta allocationMeta;// `allocationMeta.bytes` refers to "net-bytes", which is _specifically_ the [allocation] component
    u$                         grossBytes;// byte count sum of _all_ allocation components; this is what we use to iterate over all allocations in the heap */
} _Korl_Heap_Linear_AllocationMeta;
#define _korl_heap_defragmentPointer_getAllocation(defragmentPointer)          (KORL_C_CAST(u8*, *((defragmentPointer).userAddressPointer)) + (defragmentPointer).userAddressByteOffset)
#define _korl_heap_defragmentPointer_setAllocation(defragmentPointer, address) *((defragmentPointer).userAddressPointer) = KORL_C_CAST(u8*, address) - (defragmentPointer).userAddressByteOffset
#if _KORL_HEAP_GENERAL_USE_OLD
korl_internal void _korl_heap_general_allocatorPagesGuard(_Korl_Heap_General* allocator)
{
#if !_KORL_HEAP_GENERAL_DEBUG && _KORL_HEAP_GENERAL_GUARD_ALLOCATOR
    const u$ allocatorBytes = sizeof(*allocator) + allocator->availablePageFlagsSize*sizeof(*(allocator->availablePageFlags));
    DWORD oldProtect;
    if(!VirtualProtect(allocator, allocatorBytes, PAGE_READWRITE | PAGE_GUARD, &oldProtect))
        korl_logLastError("VirtualProtect failed!");
    korl_assert(oldProtect == PAGE_READWRITE);
#endif
}
korl_internal void _korl_heap_general_allocatorPagesUnguard(_Korl_Heap_General* allocator)
{
#if !_KORL_HEAP_GENERAL_DEBUG && _KORL_HEAP_GENERAL_GUARD_ALLOCATOR
    DWORD oldProtect;
    /* unguard the first page to find the total # of allocator pages */
    if(!VirtualProtect(allocator, sizeof(*allocator), PAGE_READWRITE, &oldProtect))
        korl_logLastError("VirtualProtect failed!");
    korl_assert(oldProtect == (PAGE_READWRITE | PAGE_GUARD));
    /* now we can unguard the rest, if necessary */
    const u$ pageBytes = korl_memory_pageBytes();
    korl_assert(sizeof(*allocator) < pageBytes);// probably not necessary at all due to OS constraints of w/e, but just sanity check to make sure the allocator struct fits in the first page!
    const u$ allocatorBytes = sizeof(*allocator) + allocator->availablePageFlagsSize*sizeof(*(allocator->availablePageFlags));
    if(allocatorBytes > pageBytes)
    {
        if(!VirtualProtect(KORL_C_CAST(u8*, allocator) + pageBytes, allocatorBytes - pageBytes, PAGE_READWRITE, &oldProtect))
            korl_logLastError("VirtualProtect failed!");
        korl_assert(oldProtect == (PAGE_READWRITE | PAGE_GUARD));
    }
#endif
}
#if _KORL_HEAP_GENERAL_DEBUG
korl_internal void _korl_heap_general_checkIntegrity(_Korl_Heap_General* allocator, u$* out_allocationCount, u$* out_occupiedPageCount)
{
    const u$ bitsPerFlagRegister   = 8*sizeof(*(allocator->availablePageFlags));
    const u$ usedPageFlagRegisters = korl_math_nextHighestDivision(allocator->allocationPages, bitsPerFlagRegister);
    const u$ pageBytes = korl_memory_pageBytes();
    korl_assert(0 == KORL_C_CAST(u$, allocator)  % korl_memory_virtualAlignBytes());
    const u$ allocatorPages = korl_math_nextHighestDivision(sizeof(*allocator) + allocator->availablePageFlagsSize*sizeof(*(allocator->availablePageFlags)), pageBytes);
    const void*const allocationRegionBegin = KORL_C_CAST(u8*, allocator) + allocatorPages*pageBytes;
    const void*const allocationRegionEnd   = KORL_C_CAST(u8*, allocationRegionBegin) + allocator->allocationPages*pageBytes;
    const u$ metaBytesRequired = sizeof(_Korl_Heap_General_AllocationMeta) + sizeof(_KORL_HEAP_GENERAL_ALLOCATION_META_SEPARATOR) - 1/*don't include null-terminator*/;
    u$ allocationCount                 = 0;
    u$ occupiedPageCount               = 0;
    u$ currentAllocationPageCount      = 0;
    const _Korl_Heap_General_AllocationMeta* allocationMeta = NULL;
    korl_assert(sizeof(*(allocator->availablePageFlags)) == sizeof(LONG64));
    for(u$ pfr = 0; pfr < usedPageFlagRegisters; pfr++)
    {
        const u$ pageFlagRegister = allocator->availablePageFlags[pfr];
        /* accumulate metrics */
        for(u8 i = 0; i < bitsPerFlagRegister; i++)
        {
            unsigned char bit = _bittest64(KORL_C_CAST(LONG64*, &pageFlagRegister), bitsPerFlagRegister - 1 - i);
            if(bit)
            {
                occupiedPageCount++;
                if(!currentAllocationPageCount)
                {
                    allocationCount++;
                    /* extract the meta data for this allocation & verify integrity */
                    const u$ pageIndex = pfr*bitsPerFlagRegister + i;
                    allocationMeta = KORL_C_CAST(_Korl_Heap_General_AllocationMeta*, 
                        KORL_C_CAST(u8*, allocationRegionBegin) + pageIndex*pageBytes);
                    korl_assert(KORL_C_CAST(const void*, allocationMeta) >= allocationRegionBegin);// we must be inside the allocation region
                    korl_assert(KORL_C_CAST(const void*, allocationMeta) <  allocationRegionEnd);  // we must be inside the allocation region
                    korl_assert(0 == KORL_C_CAST(u$, allocationMeta) % pageBytes);// we must be page-aligned
                    korl_assert(0 == korl_memory_compare(allocationMeta + 1, _KORL_HEAP_GENERAL_ALLOCATION_META_SEPARATOR, sizeof(_KORL_HEAP_GENERAL_ALLOCATION_META_SEPARATOR) - 1/*don't include null-terminator*/));
                }
                currentAllocationPageCount++;
            }
            else
            {
                if(currentAllocationPageCount)
                    /* finish verifying the current allocation integrity, now that we know where it should end */
                    korl_assert(allocationMeta->pagesCommitted == currentAllocationPageCount);
                currentAllocationPageCount = 0;
                allocationMeta             = NULL;
            }
        }
    }
    *out_allocationCount   = allocationCount;
    *out_occupiedPageCount = occupiedPageCount;
}
#endif
/** \return \c pageCount if there was no occupied pages in the range 
 * \c [pageRangeStartIndex,pageRangeStartIndex+pageCount) */
korl_internal u$ _korl_heap_general_occupiedPageOffset(_Korl_Heap_General* allocator, u$ pageRangeStartIndex, u$ pageCount, bool highest)
{
    korl_assert(pageCount > 0);
    korl_assert(pageRangeStartIndex + pageCount <= allocator->allocationPages);
    /* iterate over the page count and test each flag, appropriately iterating 
        the flag register & index along the way */
    const u$ bitsPerFlagRegister = 8*sizeof(*(allocator->availablePageFlags));
    u$ occupiedPageOffset = pageCount;
    //KORL-PERFORMANCE-000-000-029: memory: general allocator: occupiedPageOffset: Intuitively, I would assume that this code is just strictly sub-optimal, since we are just stupidly iterating over bit flags manually.
    for(u$ p = pageRangeStartIndex; p < pageRangeStartIndex + pageCount; p++)
    {
        const u$ pageFlagRegister = p / bitsPerFlagRegister;// the array index into allocator->availablePageFlags
        const u$ flagIndex        = p % bitsPerFlagRegister;// the bit index into this specific element of allocator->availablePageFlags
        //KORL-PERFORMANCE-000-000-030: memory: general allocator: does using bittest intrinsic result in better performance?
        if(allocator->availablePageFlags[pageFlagRegister] & (1LLU<<(bitsPerFlagRegister - 1 - flagIndex)))
        {
            occupiedPageOffset = p - pageRangeStartIndex;
            if(!highest)
                break;
        }
    }
    return occupiedPageOffset;
}
korl_internal u$ _korl_heap_general_highestOccupiedPageOffset(_Korl_Heap_General* allocator, u$ pageRangeStartIndex, u$ pageCount)
{
    return _korl_heap_general_occupiedPageOffset(allocator, pageRangeStartIndex, pageCount, true/*we want the _highest_ occupied page offset*/);
}
korl_internal u$ _korl_heap_general_lowestOccupiedPageOffset(_Korl_Heap_General* allocator, u$ pageRangeStartIndex, u$ pageCount)
{
    return _korl_heap_general_occupiedPageOffset(allocator, pageRangeStartIndex, pageCount, false/*we want the _lowest_ occupied page offset*/);
}
korl_internal void _korl_heap_general_setPageFlags(_Korl_Heap_General* allocator, u$ pageRangeStartIndex, u$ pageCount, bool pageFlagState)
{
    korl_assert(pageRangeStartIndex < allocator->allocationPages);
    korl_assert(pageRangeStartIndex + pageCount <= allocator->allocationPages);
    /* find the page flag register index & the flag index within the register */
    const u$ bitsPerFlagRegister = 8*sizeof(*(allocator->availablePageFlags));
    for(u$ p = pageRangeStartIndex; p < pageRangeStartIndex + pageCount; p++)
    {
        const u$ pageFlagRegister = p / bitsPerFlagRegister;// the array index into allocator->availablePageFlags
        const u$ flagIndex        = p % bitsPerFlagRegister;// the bit index into this specific element of allocator->availablePageFlags
        if(pageFlagState)
            allocator->availablePageFlags[pageFlagRegister] |=   1LLU<<(bitsPerFlagRegister - 1 - flagIndex);
        else
            allocator->availablePageFlags[pageFlagRegister] &= ~(1LLU<<(bitsPerFlagRegister - 1 - flagIndex));
    }
}
korl_internal u$ _korl_heap_general_occupyAvailablePages(_Korl_Heap_General* allocator, u$ pageCount)
{
    //KORL-ISSUE-000-000-088: memory: currently, it is possible for general allocator to occupy page flags that lie outside the allocator's range of allocationPages
    u$ allocationPageIndex = allocator->allocationPages;
    korl_shared_const u8 SURROUNDING_GUARD_PAGE_COUNT = 1;
    const u8 bitsPerRegister        = /*bitsPerRegister*/(8*sizeof(*(allocator->availablePageFlags)));
    const u$ fullRegistersRequired  = (pageCount + 2*SURROUNDING_GUARD_PAGE_COUNT/*leave room for one guard pages on either side of the allocation*/) / bitsPerRegister;
    const u$ remainderFlagsRequired = (pageCount + 2*SURROUNDING_GUARD_PAGE_COUNT/*leave room for one guard pages on either side of the allocation*/) % bitsPerRegister;
    const u$ remainderFlagsMask     = (~(~0ULL << remainderFlagsRequired)) << (bitsPerRegister - remainderFlagsRequired);// the remainder mask aligned to the most significant bit
    const u$ usedPageFlagRegisters  = korl_math_nextHighestDivision(allocator->allocationPages, (8*sizeof(*allocator->availablePageFlags))/*bitsPerPageFlagRegister*/);
    u$ consecutiveEmptyRegisters = 0;
    for(u$ cr = 0; cr < usedPageFlagRegisters; cr++)
    {
        const u$ rWrapped = (allocator->lastPageFlagRegisterIndex + cr) % usedPageFlagRegisters;
        if(rWrapped == 0)
            consecutiveEmptyRegisters = 0;
        if(0 != allocator->availablePageFlags[rWrapped])
            consecutiveEmptyRegisters = 0;
        else
            consecutiveEmptyRegisters++;
        if(fullRegistersRequired > 0)
        {
            if(consecutiveEmptyRegisters < fullRegistersRequired)
                continue;
            u$ fullRegistersToBeTaken      = fullRegistersRequired;
            u$ fullRegistersToBeTakenStart = rWrapped + 1 - fullRegistersRequired;
            /* we need to low-to-high-bitscan the preceding register (if it exists), 
                and high-to-low-bitscan the next register (if it exits) 
                in order to see if we can occupy this region */
            u32 precedingBits  = 0;
            u32 proceedingBits = 0;
            if(fullRegistersToBeTakenStart > 0)// there _is_ a preceding register to the consecutive empty registers
            {
                DWORD bitScanIndex = bitsPerRegister;
                if(BitScanForward64(&bitScanIndex, allocator->availablePageFlags[rWrapped - fullRegistersToBeTaken]))
                    precedingBits = bitScanIndex/*bit indices start at 0, and we want the quantity of zeroes (the bitScanIndex is the index of the first 1!)*/;
                else
                    precedingBits = bitsPerRegister;// we can occupy the entire preceding register
            }
            if(rWrapped < allocator->allocationPages - 1)// there _is_ a next register after the consecutive empty registers
            {
                DWORD bitScanIndex = bitsPerRegister;
                if(BitScanReverse64(&bitScanIndex, allocator->availablePageFlags[rWrapped + 1]))
                    proceedingBits = bitsPerRegister - 1 - bitScanIndex;// total # of leading 0s, as bitScanIndex is the offset from the least-significant bit of the most-significant 1
                else
                    proceedingBits = bitsPerRegister;// we can occupy the entire proceeding register
            }
            /* check if we have enough preceding/proceeding flags; if not, we need to keep searching */
            const u$ extendedEmptyPageFlags = precedingBits + proceedingBits;
            if(extendedEmptyPageFlags < remainderFlagsRequired)
                continue;
            /* at this point, we _know_ that we are going to be able to occupy 
                open space in/around register r! */
            /* remove guard flags from the preceding page; if there are not 
                enough, the first full register becomes the preceding page */
            if(precedingBits >= SURROUNDING_GUARD_PAGE_COUNT)
                precedingBits -= SURROUNDING_GUARD_PAGE_COUNT;
            else
            {
                fullRegistersToBeTaken--;
                fullRegistersToBeTakenStart++;
                precedingBits = bitsPerRegister - SURROUNDING_GUARD_PAGE_COUNT;
            }
            /* remove guard flags from the proceeding page; if there are not 
                enough, the last full register becomes the proceeding page */
            if(proceedingBits >= SURROUNDING_GUARD_PAGE_COUNT)
                proceedingBits -= SURROUNDING_GUARD_PAGE_COUNT;
            else
            {
                fullRegistersToBeTaken--;
                proceedingBits = bitsPerRegister - SURROUNDING_GUARD_PAGE_COUNT;
            }
            /* our new occupy index is now the beginning of the bits that 
                precede the "full register range" */
            const u$ occupyIndex = bitsPerRegister*fullRegistersToBeTakenStart - precedingBits;
            u$ remainingFlagsToOccupy = pageCount;
            if(precedingBits)
            {
                const u$ mask = precedingBits < bitsPerRegister // C99 standard §6.5.7.4; overflow left shift is undefined behavior
                    ? ~((~0ULL) << precedingBits)
                    : (korl_assert(precedingBits == bitsPerRegister), ~0ULL);
                korl_assert(fullRegistersToBeTakenStart > 0);
                korl_assert(0 == (allocator->availablePageFlags[fullRegistersToBeTakenStart - 1] & mask));
                allocator->availablePageFlags[fullRegistersToBeTakenStart - 1] |= mask;
                remainingFlagsToOccupy -= precedingBits;
            }
            /* occupy all the full registers required, then any leftover flags */
            u$ rr = fullRegistersToBeTakenStart;
            while(remainingFlagsToOccupy)
            {
                const u$ flags = KORL_MATH_MIN(remainingFlagsToOccupy, bitsPerRegister);
                const u$ mask  = flags < bitsPerRegister // C99 standard §6.5.7.4; overflow left shift is undefined behavior
                    ? (~(~0ULL << flags)) << (bitsPerRegister - flags)
                    : (korl_assert(flags == bitsPerRegister), ~0ULL);
                remainingFlagsToOccupy -= flags;
                korl_assert(rr < allocator->allocationPages);
                korl_assert(0 == (allocator->availablePageFlags[rr] & mask));
                allocator->availablePageFlags[rr++] |= mask;
            }
            /* and finally, return the page index of the first occupied page */
            allocator->lastPageFlagRegisterIndex = rr;
            allocationPageIndex = occupyIndex;
            goto returnAllocationPageIndex;
        }
        else/* we can fit all the flags within a single flag register, or 2 if we're at an offset and have spillover flags to the next register */
        {
            /* iterate over each offset in the flag register */
            for(u8 i = 0; i < bitsPerRegister; i++)
            {
                /* if the entire mask fits in the register, we just need to do a bitwise & */
                if(i + remainderFlagsRequired <= bitsPerRegister)
                {
                    if(!(allocator->availablePageFlags[rWrapped] & (remainderFlagsMask >> i)))
                    {
                        /* occupy the pages; excluding the first bits since they are reserved as guard pages */
                        const u$ occupyPageCount = remainderFlagsRequired - 2*SURROUNDING_GUARD_PAGE_COUNT;
                        const u$ occupyFlagsMask = (~(~0ULL << occupyPageCount)) << (bitsPerRegister - occupyPageCount);// flags without guard page flags aligned to the most significant bit
                        korl_assert(0 == (allocator->availablePageFlags[rWrapped] & occupyFlagsMask >> (i + SURROUNDING_GUARD_PAGE_COUNT)));
                        allocator->availablePageFlags[rWrapped] |= occupyFlagsMask >> (i + SURROUNDING_GUARD_PAGE_COUNT);
                        allocator->lastPageFlagRegisterIndex = rWrapped;
                        allocationPageIndex = rWrapped*bitsPerRegister + i + SURROUNDING_GUARD_PAGE_COUNT;
                        goto returnAllocationPageIndex;
                    }
                    continue;
                }
                /* if the mask doesn't fit in the register & there are no more registers, we're done */
                if(rWrapped >= allocator->allocationPages - 1)
                    break;
                /* otherwise, there is a next register, and the mask spills over to the next one, so we need to do two flag register operator& operations */
                const u$ spillFlags = (i + remainderFlagsRequired) % bitsPerRegister;
                const u$ spillMask  = (~(~0ULL << spillFlags)) << (bitsPerRegister - spillFlags);
                if(   !(allocator->availablePageFlags[rWrapped    ] & (remainderFlagsMask >> i))
                   && !(allocator->availablePageFlags[rWrapped + 1] & spillMask))
                {
                    /* generate masks without the leading/trailing guard pages taken into account */
                    const u$ occupyPageCount  = remainderFlagsRequired - 2*SURROUNDING_GUARD_PAGE_COUNT;
                    const u$ occupySpillFlags = (i + SURROUNDING_GUARD_PAGE_COUNT + occupyPageCount) % bitsPerRegister;// # of flags that spill over to the next flag register
                    const u$ occupySpillMask  = (~(~0ULL << occupySpillFlags)) << (bitsPerRegister - occupySpillFlags);// the spillover flag mask aligned to the most-significant bit (exactly what we need!)
                    const u$ leadingMask      = ~(~0ULL << (occupyPageCount - occupySpillFlags));
                    /* occupy the pages at the adjusted offset */
                    korl_assert(0 == (allocator->availablePageFlags[rWrapped    ] & leadingMask));
                    korl_assert(0 == (allocator->availablePageFlags[rWrapped + 1] & occupySpillMask));
                    allocator->availablePageFlags[rWrapped    ] |= leadingMask;
                    allocator->availablePageFlags[rWrapped + 1] |= occupySpillMask;
                    allocator->lastPageFlagRegisterIndex = rWrapped + 1;
                    allocationPageIndex = rWrapped*bitsPerRegister + i + SURROUNDING_GUARD_PAGE_COUNT;
                    goto returnAllocationPageIndex;
                }
            }
        }
    }
    returnAllocationPageIndex:
    return allocationPageIndex;
}
#if _KORL_HEAP_GENERAL_LAZY_COMMIT_PAGES
korl_internal void _korl_heap_general_growCommitPages(_Korl_Heap_General* allocator, u$ requestedCommitPages)
{
    const u$ allocatorPages = korl_math_nextHighestDivision(sizeof(*allocator) + allocator->availablePageFlagsSize*sizeof(*(allocator->availablePageFlags)), korl_memory_pageBytes());
    if(   allocator->allocationPagesCommitted < requestedCommitPages 
       && allocator->allocationPagesCommitted < allocator->allocationPages)
    {
        /* commit more pages than necessary (to minimize commits), while keeping 
            the total commit count within the allocation page range */
        const u$ allocationPagesCommittedNew = KORL_MATH_MIN(KORL_MATH_MAX(requestedCommitPages, 
                                                                           2*allocator->allocationPagesCommitted), 
                                                             allocator->allocationPages);
        const LPVOID resultVirtualAllocCommit = VirtualAlloc(KORL_C_CAST(u8*, allocator) + (allocatorPages + allocator->allocationPagesCommitted)*korl_memory_pageBytes(), 
                                                             (allocationPagesCommittedNew - allocator->allocationPagesCommitted)*korl_memory_pageBytes(), 
#if !_KORL_HEAP_GENERAL_GUARD_UNUSED_ALLOCATION_PAGES
                                                             MEM_COMMIT, PAGE_READWRITE);
#else
                                                             MEM_COMMIT, PAGE_READWRITE | PAGE_GUARD);
#endif
        if(resultVirtualAllocCommit == NULL)
            korl_logLastError("VirtualAlloc commit failed!");
        allocator->allocationPagesCommitted = allocationPagesCommittedNew;
    }
}
#endif
korl_internal _Korl_Heap_General* _korl_heap_general_create(u$ bytes, void* address)
{
    korl_assert(bytes > 0);
    _Korl_Heap_General* result = NULL;
    const u$ pageBytes                     = korl_memory_pageBytes();
    const u$ pagesRequired                 = korl_math_nextHighestDivision(bytes, pageBytes);
    const u$ allocatorBytesRequiredMinimum = sizeof(*result);
    /* we need to figure out the minimum # of allocator pages we need in order 
        to be able to store an array of flags used to keep track of all the 
        allocation pages */
    u$ allocatorPages = korl_math_nextHighestDivision(allocatorBytesRequiredMinimum, pageBytes);
    for(;; allocatorPages++)
    {
        korl_assert(pagesRequired >= allocatorPages + 2);// we need at least 2 extra pages to be able to make a single allocation
        const u$ allocationPages            = pagesRequired - allocatorPages;
        const u$ bytesAvailableForPageFlags = allocatorPages*pageBytes - allocatorBytesRequiredMinimum;
        const u$ availablePageFlagsSizeMax  = bytesAvailableForPageFlags / sizeof(*(result->availablePageFlags));// round down by the size of each flag register just to make things easier (negligible wasted memory anyway)
        const u$ availablePageFlagBitsMax   = (8*sizeof(*(result->availablePageFlags)))/*bitsPerPageFlagRegister*/*availablePageFlagsSizeMax;
        if(availablePageFlagBitsMax >= allocationPages)
            break;
    }
    /* now that we know how many pages are required for the allocator data 
        structure, we can calculate the final size of the available page flags 
        array (same calculations as above); we will later be able to determine 
        how many allocator pages there are based on the size of the available 
        page flags array member */
    const u$ allocationPages            = pagesRequired - allocatorPages;
    const u$ bytesAvailableForPageFlags = allocatorPages*pageBytes - allocatorBytesRequiredMinimum;
    const u$ availablePageFlagsSizeMax  = bytesAvailableForPageFlags / sizeof(*(result->availablePageFlags));// round down by the size of each flag register just to make things easier (negligible wasted memory anyway)
    /* reserve the full range of virtual memory */
    LPVOID resultVirtualAlloc = VirtualAlloc(address, pagesRequired*pageBytes, MEM_RESERVE, PAGE_NOACCESS);
    if(resultVirtualAlloc == NULL)
        korl_logLastError("VirtualAlloc failed!");
    /* commit the first pages for the allocator itself */
    // const u$ allocatorPages = korl_math_nextHighestDivision(sizeof(*result), korl_memory_pageBytes());
    const u$ allocatorBytes = allocatorPages * pageBytes;
    resultVirtualAlloc = VirtualAlloc(resultVirtualAlloc, allocatorBytes, MEM_COMMIT, PAGE_READWRITE);
    if(resultVirtualAlloc == NULL)
        korl_logLastError("VirtualAlloc failed!");
    /* initialize the memory of the allocator userData */
    korl_shared_const i8 ALLOCATOR_PAGE_MEMORY_PATTERN[] = "KORL-GeneralAllocator-Page|";
    _Korl_Heap_General*const allocator = KORL_C_CAST(_Korl_Heap_General*, resultVirtualAlloc);
    result = allocator;
    // fill the allocator page with a readable pattern for easier debugging //
    for(u$ currFillByte = 0; currFillByte < allocatorBytes; )
    {
        const u$ remainingBytes = allocatorBytes - currFillByte;
        const u$ availableFillBytes = KORL_MATH_MIN(remainingBytes, sizeof(ALLOCATOR_PAGE_MEMORY_PATTERN) - 1/*do not copy NULL-terminator*/);
        korl_memory_copy(KORL_C_CAST(u8*, allocator) + currFillByte, ALLOCATOR_PAGE_MEMORY_PATTERN, availableFillBytes);
        currFillByte += availableFillBytes;
    }
    const u$ usedPageFlagRegisters = korl_math_nextHighestDivision(allocationPages, (8*sizeof(*result->availablePageFlags))/*bitsPerPageFlagRegister*/);
    korl_memory_zero(allocator, sizeof(*result) + usedPageFlagRegisters*sizeof(*(result->availablePageFlags)));
    allocator->allocationPages        = allocationPages;
    allocator->availablePageFlagsSize = availablePageFlagsSizeMax;
    /* guard the allocator pages */
    _korl_heap_general_allocatorPagesGuard(allocator);
    return result;
}
#endif
korl_internal _Korl_Heap_Linear* _korl_heap_linear_create(u$ bytes, void* address)
{
    _Korl_Heap_Linear* result = NULL;
    const u$ heapBytes    = sizeof(*result) + 2*_KORL_HEAP_SENTINEL_PADDING_BYTES;
    const u$ virtualPages = korl_math_nextHighestDivision(bytes, korl_memory_pageBytes());
    const u$ heapPages    = korl_math_nextHighestDivision(heapBytes, korl_memory_pageBytes());
    korl_assert(bytes > heapBytes);
    void* resultVirtualAlloc = VirtualAlloc(address, virtualPages * korl_memory_pageBytes(), MEM_RESERVE, PAGE_READWRITE);
    if(!resultVirtualAlloc)
        korl_logLastError("VirtualReserve failed");
    resultVirtualAlloc = VirtualAlloc(resultVirtualAlloc, heapPages * korl_memory_pageBytes(), MEM_COMMIT, PAGE_READWRITE);
    if(!resultVirtualAlloc)
        korl_logLastError("MemoryCommit failed");
    korl_memory_set(resultVirtualAlloc, _KORL_HEAP_BYTE_PATTERN_SENTINEL, _KORL_HEAP_SENTINEL_PADDING_BYTES);
    result = KORL_C_CAST(_Korl_Heap_Linear*, KORL_C_CAST(u8*, resultVirtualAlloc) + _KORL_HEAP_SENTINEL_PADDING_BYTES);
    result->virtualPages   = virtualPages;
    result->committedPages = heapPages;
    korl_memory_set(result + 1, _KORL_HEAP_BYTE_PATTERN_SENTINEL, _KORL_HEAP_SENTINEL_PADDING_BYTES);
    return result;
}
#if _KORL_HEAP_GENERAL_USE_OLD
korl_internal _Korl_Heap_General* korl_heap_general_create(const Korl_Heap_CreateInfo* createInfo)
{
    if(createInfo->heapDescriptorCount <= 0)
        return _korl_heap_general_create(createInfo->initialHeapBytes, NULL);
    _Korl_Heap_General* result = NULL;
    _Korl_Heap_General** currentHeapList = &result;
    for(u32 h = 0; h < createInfo->heapDescriptorCount; h++)
    {
        u$*const virtualAddressStart =   KORL_C_CAST(u$*, KORL_C_CAST(      u8*, createInfo->heapDescriptors) + (h*createInfo->heapDescriptorStride) + createInfo->heapDescriptorOffset_virtualAddressStart);
        const u$ virtualBytes        = *(KORL_C_CAST(u$*, KORL_C_CAST(const u8*, createInfo->heapDescriptors) + (h*createInfo->heapDescriptorStride) + createInfo->heapDescriptorOffset_virtualBytes));
        *currentHeapList = _korl_heap_general_create(virtualBytes, KORL_C_CAST(void*, *virtualAddressStart));
        korl_assert(!"@TODO: utilize createInfo->heapDescriptorOffset_committedBytes");
        _korl_heap_general_allocatorPagesUnguard(*currentHeapList);// unguard the heap so that we can modify its next pointer if we need to
        currentHeapList = &((*currentHeapList)->next);
    }
    /* guard all the heaps now that the linked list is complete */
    for(_Korl_Heap_General* heap = result; heap;)
    {
        _Korl_Heap_General*const heapNext = heap->next;
        _korl_heap_general_allocatorPagesGuard(heap);
        heap = heapNext;
    }
    return result;
}
korl_internal void korl_heap_general_destroy(_Korl_Heap_General* allocator)
{
    _korl_heap_general_allocatorPagesUnguard(allocator);
    if(allocator->next)
        korl_heap_general_destroy(allocator->next);
    if(!VirtualFree(allocator, 0/*release everything*/, MEM_RELEASE))
        korl_logLastError("VirtualFree failed!");
}
korl_internal void korl_heap_general_empty(_Korl_Heap_General* allocator)
{
    /* sanity checks */
    _korl_heap_general_allocatorPagesUnguard(allocator);
    const u$ pageBytes = korl_memory_pageBytes();
    korl_assert(0 == KORL_C_CAST(u$, allocator) % korl_memory_virtualAlignBytes());
    /* recursively empty all allocators in the `next` linked list */
    if(allocator->next)
        korl_heap_general_empty(allocator->next);
    /* eliminate all allocations */
    const u$ allocatorPages = korl_math_nextHighestDivision(sizeof(*allocator) + allocator->availablePageFlagsSize*sizeof(*(allocator->availablePageFlags)), pageBytes);
    void*const allocationRegionBegin = KORL_C_CAST(u8*, allocator) + allocatorPages*pageBytes;
    // const void*const allocationRegionEnd   = KORL_C_CAST(u8*, allocationRegionBegin) + allocator->allocationPages*pageBytes;
#if _KORL_HEAP_GENERAL_LAZY_COMMIT_PAGES
    if(allocator->allocationPagesCommitted)
    {
#if _KORL_HEAP_GENERAL_GUARD_UNUSED_ALLOCATION_PAGES
        /* unguard all committed pages; ignore the previous guard status */
        DWORD oldProtect;
        if(!VirtualProtect(allocationRegionBegin, allocator->allocationPagesCommitted*pageBytes, PAGE_READWRITE, &oldProtect))
            korl_logLastError("VirtualProtect unguard failed!");
#endif
        /* zero out the entire region of committed allocation pages */
        korl_memory_zero(allocationRegionBegin, allocator->allocationPagesCommitted*pageBytes);
#if _KORL_HEAP_GENERAL_GUARD_UNUSED_ALLOCATION_PAGES
        /* guard entire region of committed allocation pages */
        if(!VirtualProtect(allocationRegionBegin, allocator->allocationPagesCommitted*pageBytes, PAGE_READWRITE | PAGE_GUARD, &oldProtect))
            korl_logLastError("VirtualProtect guard failed!");
        korl_assert(oldProtect == PAGE_READWRITE);
#endif
    }
#else
    /* MSDN page for VirtualFree says: 
        "The function does not fail if you attempt to decommit an uncommitted 
        page. This means that you can decommit a range of pages without first 
        determining the current commitment state."  It would be really easy to 
        just do this and re-commit pages each time we allocate, this is 
        apparently expensive, so we might not want to do this. */
    //KORL-PERFORMANCE-000-000-027: memory: uncertain performance characteristics associated with re-committed pages
    if(!VirtualFree(allocationRegionBegin, allocator->allocationPages*pageBytes, MEM_DECOMMIT))
        korl_logLastError("VirtualFree failed!");
#endif
    /* update the allocator metrics; clear all the allocation page flags */
    const u$ usedPageFlagRegisters = korl_math_nextHighestDivision(allocator->allocationPages, (8*sizeof(*allocator->availablePageFlags))/*bitsPerPageFlagRegister*/);
    korl_memory_zero(KORL_C_CAST(u8*, allocator) + sizeof(*allocator), usedPageFlagRegisters*sizeof(*(allocator->availablePageFlags)));
    /**/
    _korl_heap_general_allocatorPagesGuard(allocator);
}
korl_internal void* korl_heap_general_allocate(_Korl_Heap_General* allocator, const wchar_t* allocatorName, u$ bytes, const wchar_t* file, int line)
{
#if _KORL_HEAP_GENERAL_DEBUG
    u$ allocationCountBegin, occupiedPageCountBegin;
    _korl_heap_general_checkIntegrity(allocator, &allocationCountBegin, &occupiedPageCountBegin);
#endif
    void* result = NULL;
    korl_assert(bytes > 0);
    /* allocate a range of pages within the allocator's available page flags */
    const u$ pageBytes         = korl_memory_pageBytes();
    const u$ metaBytesRequired = sizeof(_Korl_Heap_General_AllocationMeta) + sizeof(_KORL_HEAP_GENERAL_ALLOCATION_META_SEPARATOR) - 1/*don't include null-terminator*/;
    const u$ allocationPages   = korl_math_nextHighestDivision(metaBytesRequired + bytes, pageBytes);
    _korl_heap_general_allocatorPagesUnguard(allocator);
    const u$ allocatorPages = korl_math_nextHighestDivision(sizeof(*allocator) + allocator->availablePageFlagsSize*sizeof(*(allocator->availablePageFlags)), pageBytes);
    u$ availablePageIndex = allocator->allocationPages;
    availablePageIndex = _korl_heap_general_occupyAvailablePages(allocator, allocationPages);
    if(   availablePageIndex                   >= allocator->allocationPages 
       || availablePageIndex + allocationPages >  allocator->allocationPages/*required only because of KORL-ISSUE-000-000-088*/)
    {
        if(   availablePageIndex                   < allocator->allocationPages
           && availablePageIndex + allocationPages > allocator->allocationPages)
            _korl_heap_general_setPageFlags(allocator, availablePageIndex, allocator->allocationPages - allocationPages, false);//required only because of KORL-ISSUE-000-000-088
        if(!allocator->next)
        {
            korl_log(WARNING, "general allocator \"%ws\" out of memory", allocatorName);
            /* we want to either (depending on which results in more pages): 
                - double the amount of allocationPages of the next allocator
                - have at _least_ the # of allocationPages required to satisfy `bytes` */
            const u$ nextHeapPages = KORL_MATH_MAX(2*allocator->allocationPages, 1/*include preceding guard page*/+allocationPages);
            allocator->next = _korl_heap_general_create((allocatorPages + nextHeapPages)*pageBytes, NULL);
        }
        result = korl_heap_general_allocate(allocator->next, allocatorName, bytes, file, line);
        if(!result)
            korl_log(ERROR, "system out of memory");
        goto guardAllocator_returnResult;
    }
    /* now that we have a set of pages which we can safely assume are unused, we 
        need to obtain the address & commit the memory */
    _Korl_Heap_General_AllocationMeta*const metaAddress = KORL_C_CAST(_Korl_Heap_General_AllocationMeta*, 
        KORL_C_CAST(u8*, allocator) + (allocatorPages + availablePageIndex)*pageBytes);
    result = KORL_C_CAST(u8*, metaAddress) + metaBytesRequired;
#if _KORL_HEAP_GENERAL_LAZY_COMMIT_PAGES
    /* if we haven't committed enough pages to the allocator yet, let's do that; 
        newly committed pages are guarded until they become occupied */
    _korl_heap_general_growCommitPages(allocator, availablePageIndex + allocationPages);
#if _KORL_HEAP_GENERAL_GUARD_UNUSED_ALLOCATION_PAGES
    /* all we have to do now is to release the protections on our now-occupied 
        allocation pages, since we should be able to guarantee that they are: 
        - committed
        - protected/guarded
        - cleared (all zero memory) */
    DWORD oldProtect;
    if(!VirtualProtect(metaAddress, allocationPages*pageBytes, PAGE_READWRITE, &oldProtect))
        korl_logLastError("VirtualProtect unguard failed!");
    korl_assert(oldProtect == (PAGE_READWRITE | PAGE_GUARD));
#endif
#else
    //KORL-PERFORMANCE-000-000-027: memory: uncertain performance characteristics associated with re-committed pages
    const LPVOID resultVirtualAllocCommit = VirtualAlloc(metaAddress, allocationPages*pageBytes, MEM_COMMIT, PAGE_READWRITE);
    if(resultVirtualAllocCommit == NULL)
        korl_logLastError("VirtualAlloc failed!");
#endif
    /* now we can initialize the memory */
    metaAddress->allocationMeta.bytes = bytes;
    metaAddress->allocationMeta.file  = file;
    metaAddress->allocationMeta.line  = line;
    metaAddress->pagesCommitted       = allocationPages;
    korl_memory_copy(metaAddress + 1, _KORL_HEAP_GENERAL_ALLOCATION_META_SEPARATOR, sizeof(_KORL_HEAP_GENERAL_ALLOCATION_META_SEPARATOR) - 1/*don't include null-terminator*/);
    guardAllocator_returnResult:
        _korl_heap_general_allocatorPagesGuard(allocator);
        #if _KORL_HEAP_GENERAL_DEBUG
            u$ allocationCountEnd, occupiedPageCountEnd;
            _korl_heap_general_checkIntegrity(allocator, &allocationCountEnd, &occupiedPageCountEnd);
            korl_assert(allocationCountEnd   == allocationCountBegin + 1);
            korl_assert(occupiedPageCountEnd == occupiedPageCountBegin + allocationPages);
        #endif
        return result;
}
korl_internal void* korl_heap_general_reallocate(_Korl_Heap_General* allocator, const wchar_t* allocatorName, void* allocation, u$ bytes, const wchar_t* file, int line)
{
    #if _KORL_HEAP_GENERAL_DEBUG
        u$ allocationCountBegin, occupiedPageCountBegin;
        _korl_heap_general_checkIntegrity(allocator, &allocationCountBegin, &occupiedPageCountBegin);
    #endif
    /* sanity checks */
    _korl_heap_general_allocatorPagesUnguard(allocator);
    const u$ pageBytes = korl_memory_pageBytes();
    korl_assert(0 == KORL_C_CAST(u$, allocator)  % korl_memory_virtualAlignBytes());
    const u$ allocatorPages = korl_math_nextHighestDivision(sizeof(*allocator) + allocator->availablePageFlagsSize*sizeof(*(allocator->availablePageFlags)), pageBytes);
    const void*const allocationRegionBegin = KORL_C_CAST(u8*, allocator) + allocatorPages*pageBytes;
    const void*const allocationRegionEnd   = KORL_C_CAST(u8*, allocationRegionBegin) + allocator->allocationPages*pageBytes;
    if(allocation < allocationRegionBegin || allocation >= allocationRegionEnd)
    {
        if(!allocator->next)
            korl_log(ERROR, "allocation not found");
        allocation = korl_heap_general_reallocate(allocator->next, allocatorName, allocation, bytes, file, line);
        goto guardAllocator_returnAllocation;
    }
    korl_assert(allocation >= allocationRegionBegin);// we must be inside the allocation region
    korl_assert(allocation <  allocationRegionEnd);  // we must be inside the allocation region
    /* access the allocation meta data */
    const u$ metaBytesRequired = sizeof(_Korl_Heap_General_AllocationMeta) + sizeof(_KORL_HEAP_GENERAL_ALLOCATION_META_SEPARATOR) - 1/*don't include null-terminator*/;
    _Korl_Heap_General_AllocationMeta*const allocationMeta = KORL_C_CAST(_Korl_Heap_General_AllocationMeta*
                                                                        ,KORL_C_CAST(u8*, allocation) - metaBytesRequired);
    #if _KORL_HEAP_GENERAL_DEBUG
        const u$ allocationPages = allocationMeta->pagesCommitted;
    #endif
    /* allocation sanity checks */
    if(0 != KORL_C_CAST(u$, allocationMeta) % pageBytes)// we must be page-aligned
        korl_log(ERROR, "invalid address alignment: 0x%p", allocation);
    if(0 != korl_memory_compare(allocationMeta + 1, _KORL_HEAP_GENERAL_ALLOCATION_META_SEPARATOR, sizeof(_KORL_HEAP_GENERAL_ALLOCATION_META_SEPARATOR) - 1/*don't include null-terminator*/))
        korl_log(ERROR, "invalid allocation metadata: 0x%p", allocation);
    /* calculate the page index of the allocation */
    const u$ allocationPage     = (KORL_C_CAST(u$, allocationMeta) - KORL_C_CAST(u$, allocationRegionBegin)) / pageBytes;
    const u$ newAllocationPages = korl_math_nextHighestDivision(metaBytesRequired + bytes, pageBytes);
    /* if we're shrinking the allocation... */
    if(newAllocationPages <= allocationMeta->pagesCommitted)
    {
        /* only if we are shrinking the # of committed pages, we must clean up pages that become unoccupied */
        if(newAllocationPages < allocationMeta->pagesCommitted)
        {
            #if _KORL_HEAP_GENERAL_LAZY_COMMIT_PAGES
                /* zero out the pages we're losing, in preparation for future allocations */
                korl_memory_zero(KORL_C_CAST(u8*, allocationMeta) + newAllocationPages*pageBytes, 
                                 (allocationMeta->pagesCommitted - newAllocationPages)*pageBytes);
                #if _KORL_HEAP_GENERAL_GUARD_UNUSED_ALLOCATION_PAGES
                    /* guard these pages */
                    DWORD oldProtect;
                    if(!VirtualProtect(KORL_C_CAST(u8*, allocationMeta) + newAllocationPages*pageBytes, 
                                       (allocationMeta->pagesCommitted - newAllocationPages)*pageBytes, 
                                       PAGE_READWRITE | PAGE_GUARD, &oldProtect))
                        korl_logLastError("VirtualProtect guard failed!");
                    korl_assert(oldProtect == PAGE_READWRITE);
                #endif
            #else
                //KORL-PERFORMANCE-000-000-027: memory: uncertain performance characteristics associated with re-committed pages
                if(!VirtualFree(KORL_C_CAST(u8*, allocationMeta) + newAllocationPages*pageBytes, (allocationMeta->pagesCommitted - newAllocationPages)*pageBytes, MEM_DECOMMIT))
                    korl_logLastError("VirtualFree failed!");
            #endif
            _korl_heap_general_setPageFlags(allocator, allocationPage + newAllocationPages, (allocationMeta->pagesCommitted - newAllocationPages), false/*unoccupied*/);
        }
        allocationMeta->pagesCommitted       = newAllocationPages;
        allocationMeta->allocationMeta.bytes = bytes;
        goto guardAllocator_returnAllocation;
    }
    /* all the code past here involves cases where we need to expand the allocation */
    korl_assert(newAllocationPages > allocationMeta->pagesCommitted);// sanity check
    /* if the allocator has enough allocation pages, and those trailing 
        allocation pages are not occupied, then expand into these pages in-place */
    if(   allocationPage + newAllocationPages <= allocator->allocationPages 
       && _korl_heap_general_lowestOccupiedPageOffset(allocator, allocationPage + allocationMeta->pagesCommitted, newAllocationPages - allocationMeta->pagesCommitted + 1/*guard page*/) 
          >= newAllocationPages - allocationMeta->pagesCommitted + 1/*guard page*/)
    {
        /* occupy the pages */
        _korl_heap_general_setPageFlags(allocator, allocationPage + allocationMeta->pagesCommitted, newAllocationPages - allocationMeta->pagesCommitted, true/*occupy*/);
        /* expand the committed pages, if necessary */
        #if _KORL_HEAP_GENERAL_LAZY_COMMIT_PAGES
            /* if we haven't committed enough pages to the allocator yet, let's do that; 
                newly committed pages are guarded until they become occupied */
            _korl_heap_general_growCommitPages(allocator, allocationPage + newAllocationPages);
            #if _KORL_HEAP_GENERAL_GUARD_UNUSED_ALLOCATION_PAGES
                /* release the protections on our now-occupied allocation pages */
                DWORD oldProtect;
                if(!VirtualProtect(KORL_C_CAST(u8*, allocationMeta) + allocationMeta->pagesCommitted*pageBytes, (newAllocationPages - allocationMeta->pagesCommitted)*pageBytes, PAGE_READWRITE, &oldProtect))
                    korl_logLastError("VirtualProtect unguard failed!");
                korl_assert(oldProtect == (PAGE_READWRITE | PAGE_GUARD));
            #endif
        #else
            //KORL-PERFORMANCE-000-000-027: memory: uncertain performance characteristics associated with re-committed pages
            const LPVOID resultVirtualAllocCommit = VirtualAlloc(allocationMeta, newAllocationPages*pageBytes, MEM_COMMIT, PAGE_READWRITE);
            if(resultVirtualAllocCommit == NULL)
                korl_logLastError("VirtualAlloc failed!");
        #endif
        /* update the meta data */
        allocationMeta->pagesCommitted       = newAllocationPages;
        allocationMeta->allocationMeta.bytes = bytes;
        allocationMeta->allocationMeta.file  = file;
        allocationMeta->allocationMeta.line  = line;
        goto guardAllocator_returnAllocation;
    }
    /* we were unable to quickly expand the allocation; we need to allocate=>copy=>free */
    /* clear our page flags */
    _korl_heap_general_setPageFlags(allocator, allocationPage, allocationMeta->pagesCommitted, false/*unoccupied*/);
    /* occupy new page flags that satisfy newAllocationPages */
    const u$ newAllocationPage = _korl_heap_general_occupyAvailablePages(allocator, newAllocationPages);
    if(   newAllocationPage                      >= allocator->allocationPages
       || newAllocationPage + newAllocationPages >  allocator->allocationPages/*required only because of KORL-ISSUE-000-000-088*/)
    {
        /* if we failed to occupy newAllocationPages, that means the allocator 
            is full; we need to create a new allocation in another heap, copy 
            the data of this allocation to the new one, and zero/guard this memory */
        if(   newAllocationPage                      < allocator->allocationPages
           && newAllocationPage + newAllocationPages > allocator->allocationPages)
            _korl_heap_general_setPageFlags(allocator, newAllocationPage, allocator->allocationPages - newAllocationPages, false);//required only because of KORL-ISSUE-000-000-088
        // create a new allocation in another heap in the heap linked list //
        if(!allocator->next)
        {
            korl_log(WARNING, "general allocator \"%ws\" out of memory", allocatorName);
            /* we want to either (depending on which results in more pages): 
                - double the amount of allocationPages of the next allocator
                - have at _least_ the # of allocationPages required to satisfy `bytes` */
            const u$ nextHeapPages = KORL_MATH_MAX(2*allocator->allocationPages, 1/*include preceding guard page*/+newAllocationPages);
            allocator->next = _korl_heap_general_create((allocatorPages + nextHeapPages)*pageBytes, NULL);
        }
        void*const newAllocation = korl_heap_general_allocate(allocator->next, allocatorName, bytes, file, line);
        if(!newAllocation)
            korl_log(ERROR, "system out of memory");
        // copy data to newAllocation //
        korl_memory_copy(newAllocation, allocation, KORL_MATH_MIN(allocationMeta->allocationMeta.bytes, bytes));
        // zero/guard old allocation memory //
        #if _KORL_HEAP_GENERAL_LAZY_COMMIT_PAGES
            /* save the old allocation's byte count, since we're about to zero this memory */
            const u$ oldAllocationBytes = allocationMeta->pagesCommitted*pageBytes;
            /* zero out the memory for future allocations to reclaim */
            korl_memory_zero(allocationMeta, metaBytesRequired + allocationMeta->allocationMeta.bytes);
            #if _KORL_HEAP_GENERAL_GUARD_UNUSED_ALLOCATION_PAGES
                /* guard the pages */
                DWORD oldProtect;
                if(!VirtualProtect(allocationMeta, oldAllocationBytes, PAGE_READWRITE | PAGE_GUARD, &oldProtect))
                    korl_logLastError("VirtualProtect guard failed!");
                korl_assert(oldProtect == PAGE_READWRITE);
            #endif
        #else
            /* decommit the pages */
            //KORL-PERFORMANCE-000-000-027: memory: uncertain performance characteristics associated with re-committed pages
            if(!VirtualFree(allocationMeta, allocationMeta->pagesCommitted*pageBytes, MEM_DECOMMIT))
                korl_logLastError("VirtualFree failed!");
        #endif
        // return with the newAllocation //
        allocation = newAllocation;
        goto guardAllocator_returnAllocation;
    }
    /* commit new pages */
    _Korl_Heap_General_AllocationMeta*const newMeta = KORL_C_CAST(_Korl_Heap_General_AllocationMeta*, 
        KORL_C_CAST(u8*, allocator) + (allocatorPages + newAllocationPage)*pageBytes);
    void*const newAllocation = KORL_C_CAST(u8*, newMeta) + metaBytesRequired;
    #if _KORL_HEAP_GENERAL_LAZY_COMMIT_PAGES
        /* grow the # of committed pages in the allocator, if necessary */
        _korl_heap_general_growCommitPages(allocator, newAllocationPage + newAllocationPages);
        #if _KORL_HEAP_GENERAL_GUARD_UNUSED_ALLOCATION_PAGES
            /* unguard the pages we now occupy */
            DWORD oldProtect;
            if(!VirtualProtect(newMeta, newAllocationPages*pageBytes, PAGE_READWRITE, &oldProtect))
                korl_logLastError("VirtualProtect unguard failed!");
            if(newMeta < allocationMeta || KORL_C_CAST(u8*, newMeta) >= KORL_C_CAST(u8*, allocationMeta) + allocationMeta->pagesCommitted*pageBytes)
                // we can only assert this old protection on the first page _if_ newMeta 
                //  lies outside the first allocation region
                korl_assert(oldProtect == (PAGE_READWRITE | PAGE_GUARD));
            else
                // inside the first allocation region, we expect the first page to 
                //  already be unguarded
                korl_assert(oldProtect == PAGE_READWRITE);
        #endif
    #else
        //KORL-PERFORMANCE-000-000-027: memory: uncertain performance characteristics associated with re-committed pages
        const LPVOID resultVirtualAllocCommit = VirtualAlloc(newMeta, newAllocationPages*pageBytes, MEM_COMMIT, PAGE_READWRITE);
        if(resultVirtualAllocCommit == NULL)
            korl_logLastError("VirtualAlloc failed!");
    #endif
    /* move data from old allocation to new allocation; move is required, as it 
        is entirely possible that the new allocation & old allocation address 
        ranges overlap! */
    const u$ allocationPageEnd    = allocationPage    + allocationMeta->pagesCommitted;// gather this data before allocationMeta gets clobbered
    const u$ newAllocationPageEnd = newAllocationPage + newAllocationPages;            // gather this data before allocationMeta gets clobbered
    korl_memory_move(newAllocation, allocation, allocationMeta->allocationMeta.bytes);
    allocation = newAllocation;// we're done w/ allocation at this point
    /* create new meta data with updated metrics */
    newMeta->pagesCommitted = newAllocationPages;
    newMeta->allocationMeta.bytes = bytes;
    newMeta->allocationMeta.file  = file;
    newMeta->allocationMeta.line  = line;
    korl_memory_copy(newMeta + 1, _KORL_HEAP_GENERAL_ALLOCATION_META_SEPARATOR, sizeof(_KORL_HEAP_GENERAL_ALLOCATION_META_SEPARATOR) - 1/*don't include null-terminator*/);
    /* Decommit pages that are outside of the new allocation pages, and inside 
        the old allocation pages.  Possible scenarios:
        - [newBegin             newEnd][oldBegin             oldEnd]// decommit required // oldEnd > newBegin && oldBegin > newEnd
        - [newBegin            [oldBegin newEnd]             oldEnd]// decommit required // oldEnd > newBegin && oldBegin < newEnd
        - [newBegin      [oldBegin             oldEnd]       newEnd]// do nothing        // 
        - [oldBegin             [newBegin oldEnd]            newEnd]// decommit required // oldEnd > newBegin && oldBegin < newEnd
        - [oldBegin             oldEnd][newBegin             newEnd]// decommit required // oldEnd < newBegin && oldBegin < newEnd
        We _know_ that since we are _expanding_ the allocation, we can never 
        encounter the following scenarios:
        - [oldBegin      [newBegin             newEnd]       oldEnd]
        - newBegin == oldBegin && newEnd == oldEnd (allocations occupy the same region) */
    if(allocationPage >= newAllocationPage && allocationPageEnd <= newAllocationPageEnd)//old allocation is contained within new allocation
        goto guardAllocator_returnAllocation;
    u$ decommitPage    = allocationPage;
    u$ decommitPageEnd = allocationPageEnd;
    if(   allocationPageEnd > newAllocationPage && allocationPageEnd > newAllocationPageEnd //    old allocation is higher than new allocation
       && allocationPage < newAllocationPageEnd)                                            //AND old allocation is intersecting
        decommitPage = newAllocationPageEnd;
    if(   allocationPage < newAllocationPage && allocationPage < newAllocationPageEnd //    old allocation is lower than new allocation
       && allocationPageEnd > newAllocationPage)                                      //AND old allocation is intersecting
        decommitPageEnd = newAllocationPage;
    korl_assert(decommitPageEnd > decommitPage);
    const u$ decommitPages = decommitPageEnd - decommitPage;
    korl_assert(decommitPages > 0);// this _must_ be the case, since we have eliminated all other possible intersection scenarios... right?
    #if _KORL_HEAP_GENERAL_LAZY_COMMIT_PAGES
        /* zero out these pages */
        korl_memory_zero(KORL_C_CAST(u8*, allocator) + (allocatorPages + decommitPage)*pageBytes, decommitPages*pageBytes);
        #if _KORL_HEAP_GENERAL_GUARD_UNUSED_ALLOCATION_PAGES
            /* guard these pages */
            if(!VirtualProtect(KORL_C_CAST(u8*, allocator) + (allocatorPages + decommitPage)*pageBytes, decommitPages*pageBytes, 
                               PAGE_READWRITE | PAGE_GUARD, &oldProtect))
                korl_logLastError("VirtualProtect guard failed!");
            korl_assert(oldProtect == PAGE_READWRITE);
        #endif
    #else
        if(!VirtualFree(KORL_C_CAST(u8*, allocator) + (allocatorPages + decommitPage)*pageBytes, decommitPages*pageBytes, MEM_DECOMMIT))
            korl_logLastError("VirtualFree failed!");
    #endif
    guardAllocator_returnAllocation:
        _korl_heap_general_allocatorPagesGuard(allocator);
        #if _KORL_HEAP_GENERAL_DEBUG
            u$ allocationCountEnd, occupiedPageCountEnd;
            _korl_heap_general_checkIntegrity(allocator, &allocationCountEnd, &occupiedPageCountEnd);
            korl_assert(allocationCountEnd   == allocationCountBegin);
            korl_assert(occupiedPageCountEnd == occupiedPageCountBegin - allocationPages + newAllocationPages);
        #endif
        return allocation;
}
korl_internal void korl_heap_general_free(_Korl_Heap_General* allocator, void* allocation, const wchar_t* file, int line)
{
#if _KORL_HEAP_GENERAL_DEBUG
    u$ allocationCountBegin, occupiedPageCountBegin;
    _korl_heap_general_checkIntegrity(allocator, &allocationCountBegin, &occupiedPageCountBegin);
#endif
    /* sanity checks */
    _korl_heap_general_allocatorPagesUnguard(allocator);
    const u$ pageBytes = korl_memory_pageBytes();
    korl_assert(0 == KORL_C_CAST(u$, allocator)  % korl_memory_virtualAlignBytes());
    const u$ allocatorPages = korl_math_nextHighestDivision(sizeof(*allocator) + allocator->availablePageFlagsSize*sizeof(*(allocator->availablePageFlags)), pageBytes);
    const void*const allocationRegionBegin = KORL_C_CAST(u8*, allocator) + allocatorPages*pageBytes;
    const void*const allocationRegionEnd   = KORL_C_CAST(u8*, allocationRegionBegin) + allocator->allocationPages*pageBytes;
    if(allocation < allocationRegionBegin || allocation >= allocationRegionEnd)
    {
        if(!allocator->next)
            korl_log(ERROR, "allocation not found");
        korl_heap_general_free(allocator->next, allocation, file, line);
        goto guardAllocator;
    }
    korl_assert(allocation >= allocationRegionBegin);// we must be inside the allocation region
    korl_assert(allocation <  allocationRegionEnd);  // we must be inside the allocation region
    /* access the allocation meta data */
    const u$ metaBytesRequired = sizeof(_Korl_Heap_General_AllocationMeta) + sizeof(_KORL_HEAP_GENERAL_ALLOCATION_META_SEPARATOR) - 1/*don't include null-terminator*/;
    _Korl_Heap_General_AllocationMeta*const allocationMeta = KORL_C_CAST(_Korl_Heap_General_AllocationMeta*, 
        KORL_C_CAST(u8*, allocation) - metaBytesRequired);
    /* allocation sanity checks */
    korl_assert(0 == KORL_C_CAST(u$, allocationMeta) % pageBytes);// we must be page-aligned
    korl_assert(0 == korl_memory_compare(allocationMeta + 1, _KORL_HEAP_GENERAL_ALLOCATION_META_SEPARATOR, sizeof(_KORL_HEAP_GENERAL_ALLOCATION_META_SEPARATOR) - 1/*don't include null-terminator*/));
    /* calculate the page index of the allocation */
    const u$ allocationPages = allocationMeta->pagesCommitted;
    const u$ allocationPage  = (KORL_C_CAST(u$, allocationMeta) - KORL_C_CAST(u$, allocationRegionBegin)) / pageBytes;
    /* clear the allocation page flags associated with this allocation */
    _korl_heap_general_setPageFlags(allocator, allocationPage, allocationMeta->pagesCommitted, false/*clear*/);
#if _KORL_HEAP_GENERAL_LAZY_COMMIT_PAGES
    /* zero out the memory for future allocations to reclaim */
    korl_memory_zero(allocationMeta, metaBytesRequired + allocationMeta->allocationMeta.bytes);
#if _KORL_HEAP_GENERAL_GUARD_UNUSED_ALLOCATION_PAGES
    /* guard the pages */
    DWORD oldProtect;
    if(!VirtualProtect(allocationMeta, allocationPages*pageBytes, PAGE_READWRITE | PAGE_GUARD, &oldProtect))
        korl_logLastError("VirtualProtect guard failed!");
    korl_assert(oldProtect == PAGE_READWRITE);
#endif
#else
    /* decommit the pages */
    //KORL-PERFORMANCE-000-000-027: memory: uncertain performance characteristics associated with re-committed pages
    if(!VirtualFree(allocationMeta, allocationMeta->pagesCommitted*pageBytes, MEM_DECOMMIT))
        korl_logLastError("VirtualFree failed!");
#endif
    guardAllocator:
        _korl_heap_general_allocatorPagesGuard(allocator);
        #if _KORL_HEAP_GENERAL_DEBUG
            u$ allocationCountEnd, occupiedPageCountEnd;
            _korl_heap_general_checkIntegrity(allocator, &allocationCountEnd, &occupiedPageCountEnd);
            korl_assert(allocationCountEnd   == allocationCountBegin - 1);
            korl_assert(occupiedPageCountEnd == occupiedPageCountBegin - allocationPages);
        #endif
}
korl_internal KORL_HEAP_ENUMERATE(korl_heap_general_enumerate)
{
    _Korl_Heap_General*const allocator = heap;
    const u$                 pageBytes = korl_memory_pageBytes();
    _korl_heap_general_allocatorPagesUnguard(allocator);
    const u$         allocatorPages    = korl_math_nextHighestDivision(sizeof(*allocator) + allocator->availablePageFlagsSize*sizeof(*(allocator->availablePageFlags)), pageBytes);
    const void*const virtualAddressEnd = KORL_C_CAST(u8*, allocator) + allocatorPages*pageBytes + allocator->allocationPages*pageBytes;
    korl_assert(!"@TODO: not implemented; should we just destroy this?");
    callback(callbackUserData, allocator, virtualAddressEnd, 0, 0);
    if(allocator->next)
        korl_heap_general_enumerate(allocator->next, callback, callbackUserData);
    _korl_heap_general_allocatorPagesGuard(allocator);
}
korl_internal KORL_HEAP_ENUMERATE_ALLOCATIONS(korl_heap_general_enumerateAllocations)
{
    _Korl_Heap_General* allocator = KORL_C_CAST(_Korl_Heap_General*, allocatorUserData);
    bool abortEnumeration = false;
    while(allocator && !abortEnumeration)
    {
        /* sanity checks */
        _korl_heap_general_allocatorPagesUnguard(allocator);
        korl_assert(0 == KORL_C_CAST(u$, allocator) % korl_memory_virtualAlignBytes());
        /**/
        const u$ pageBytes             = korl_memory_pageBytes();
        const u$ allocatorPages        = korl_math_nextHighestDivision(sizeof(*allocator) + allocator->availablePageFlagsSize*sizeof(*(allocator->availablePageFlags)), pageBytes);
        const u$ bitsPerFlagRegister   = 8*sizeof(*(allocator->availablePageFlags));
        const u$ usedPageFlagRegisters = korl_math_nextHighestDivision(allocator->allocationPages, bitsPerFlagRegister);
        u$ pageFlagsRemainder = 0;
        for(u$ pfr = 0; pfr < usedPageFlagRegisters; pfr++)
        {
            u$ pageFlagRegister = allocator->availablePageFlags[pfr];
            /* remove page flag bits that are remaining from previous register(s) */
            const u$ pageFlagsRemainderInRegister     = KORL_MATH_MIN(bitsPerFlagRegister, pageFlagsRemainder);
            const u$ pageFlagsRemainderInRegisterMask = pageFlagsRemainderInRegister < bitsPerFlagRegister // C99 standard §6.5.7.4; overflow left shift is undefined behavior
                                                        ? ~((~0LLU) << pageFlagsRemainderInRegister) 
                                                        : ~0LLU;
            pageFlagRegister &= ~(pageFlagsRemainderInRegisterMask << (bitsPerFlagRegister - pageFlagsRemainderInRegister));
            pageFlagsRemainder -= pageFlagsRemainderInRegister;
            /**/
            unsigned long mostSignificantSetBitIndex = korl_checkCast_u$_to_u32(bitsPerFlagRegister)/*smallest invalid value*/;
            while(_BitScanReverse64(&mostSignificantSetBitIndex, pageFlagRegister))
            {
                const u$ allocationPageIndex = pfr*bitsPerFlagRegister + (bitsPerFlagRegister - 1 - mostSignificantSetBitIndex);
                if(allocationPageIndex >= allocator->allocationPages)
                    break;// @TODO: it's possible that our pageFlagRegisters contain more bits than we have actual allocation pages, and it's also currently possible that our shitty allocator is raising these flags; we need to ignore these page flags, as they are outside the bounds of the allocator & therefore cannot possibly be valid
                const _Korl_Heap_General_AllocationMeta*const metaAddress = KORL_C_CAST(_Korl_Heap_General_AllocationMeta*, 
                    KORL_C_CAST(u8*, allocator) + (allocatorPages + allocationPageIndex)*pageBytes);
                const u$ metaBytesRequired = sizeof(*metaAddress) + sizeof(_KORL_HEAP_GENERAL_ALLOCATION_META_SEPARATOR) - 1/*don't include null-terminator*/;
                const u$ grossBytes = 0;// @TODO: actually calculate this? or maybe we dump this allocator :')
                const u$ netBytes   = 0;// @TODO: actually calculate this? or maybe we dump this allocator :')
                if(!callback(callbackUserData, KORL_C_CAST(u8*, metaAddress) + metaBytesRequired, &(metaAddress->allocationMeta), grossBytes, netBytes))
                {
                    abortEnumeration = true;
                    goto guardAllocator;
                }
                const u$ maxFlagsInRegister        = mostSignificantSetBitIndex + 1;
                const u$ allocationFlagsInRegister = KORL_MATH_MIN(maxFlagsInRegister, metaAddress->pagesCommitted);
                /* remove the flags that this allocation occupies */
                const u$ allocationFlagsInRegisterMask = allocationFlagsInRegister < bitsPerFlagRegister
                                                         ? ~((~0LLU) << allocationFlagsInRegister) // C99 standard §6.5.7.4; overflow left shift is undefined behavior
                                                         : ~0LLU;
                pageFlagRegister &= ~(allocationFlagsInRegisterMask << (mostSignificantSetBitIndex - (allocationFlagsInRegister - 1)));
                /* carry over any remaining flags that will occupy future register(s) */
                pageFlagsRemainder += metaAddress->pagesCommitted - allocationFlagsInRegister;
            }
            //KORL-PERFORMANCE-000-000-028: memory: (likely extremely minor): _*general_enumerateAllocations: we should be able to skip more page flag registers immediately if we know we occupy 1 or more full registers
        }
        guardAllocator:
            _Korl_Heap_General*const nextAllocator = allocator->next;// we need to do this nonsense because we cannot access `allocator->next` once the allocator page(s) are guarded!
            _korl_heap_general_allocatorPagesGuard(allocator);
            allocator = nextAllocator;
    }
}
#else
korl_internal _Korl_Heap_General* korl_heap_general_create(const Korl_Heap_CreateInfo* createInfo)
{
    korl_assert(!"@TODO");
    return NULL;
}
korl_internal void korl_heap_general_destroy(_Korl_Heap_General* allocator)
{
    korl_assert(!"@TODO");
}
korl_internal void korl_heap_general_empty(_Korl_Heap_General* allocator)
{
    korl_assert(!"@TODO");
}
korl_internal void* korl_heap_general_allocate(_Korl_Heap_General* allocator, const wchar_t* allocatorName, u$ bytes, const wchar_t* file, int line)
{
    korl_assert(!"@TODO");
    return NULL;
}
korl_internal void* korl_heap_general_reallocate(_Korl_Heap_General* allocator, const wchar_t* allocatorName, void* allocation, u$ bytes, const wchar_t* file, int line)
{
    korl_assert(!"@TODO");
    return NULL;
}
korl_internal void korl_heap_general_free(_Korl_Heap_General* allocator, void* allocation, const wchar_t* file, int line)
{
    korl_assert(!"@TODO");
}
korl_internal KORL_HEAP_ENUMERATE(korl_heap_general_enumerate)
{
    korl_assert(!"@TODO");
}
korl_internal KORL_HEAP_ENUMERATE_ALLOCATIONS(korl_heap_general_enumerateAllocations)
{
    korl_assert(!"@TODO");
}
#endif
korl_internal _Korl_Heap_Linear* korl_heap_linear_create(const Korl_Heap_CreateInfo* createInfo)
{
    if(createInfo->heapDescriptorCount <= 0)
        return _korl_heap_linear_create(createInfo->initialHeapBytes, NULL);
    _Korl_Heap_Linear* result = NULL;
    _Korl_Heap_Linear** currentHeapList = &result;
    for(u32 h = 0; h < createInfo->heapDescriptorCount; h++)
    {
        u$*const virtualAddressStart =   KORL_C_CAST(u$*, KORL_C_CAST(      u8*, createInfo->heapDescriptors) + (h*createInfo->heapDescriptorStride) + createInfo->heapDescriptorOffset_virtualAddressStart);
        const u$ virtualBytes        = *(KORL_C_CAST(u$*, KORL_C_CAST(const u8*, createInfo->heapDescriptors) + (h*createInfo->heapDescriptorStride) + createInfo->heapDescriptorOffset_virtualBytes));
        const u$ committedBytes      = *(KORL_C_CAST(u$*, KORL_C_CAST(const u8*, createInfo->heapDescriptors) + (h*createInfo->heapDescriptorStride) + createInfo->heapDescriptorOffset_committedBytes));
        *currentHeapList = _korl_heap_linear_create(virtualBytes, KORL_C_CAST(void*, *virtualAddressStart));
        const void*const resultCommit = VirtualAlloc(KORL_C_CAST(void*, *virtualAddressStart), committedBytes, MEM_COMMIT, PAGE_READWRITE);
        if(!resultCommit)
            korl_logLastError("memory commit failed");
        currentHeapList = &((*currentHeapList)->next);
    }
    return result;
}
korl_internal void korl_heap_linear_destroy(_Korl_Heap_Linear*const allocator)
{
    if(allocator->next)
        korl_heap_linear_destroy(allocator->next);
    if(!VirtualFree(allocator, 0/*release everything*/, MEM_RELEASE))
        korl_logLastError("VirtualFree failed!");
}
korl_internal void korl_heap_linear_empty(_Korl_Heap_Linear*const allocator)
{
    const u$ heapBytes       = 2 * _KORL_HEAP_SENTINEL_PADDING_BYTES + sizeof(*allocator);
    u8*const heapVirtualBase = KORL_C_CAST(u8*, allocator) - _KORL_HEAP_SENTINEL_PADDING_BYTES;
    korl_memory_set(heapVirtualBase + heapBytes, _KORL_HEAP_BYTE_PATTERN_FREE, allocator->allocatedBytes);
    allocator->allocatedBytes = 0;
}
korl_internal void* korl_heap_linear_allocate(_Korl_Heap_Linear*const allocator, const wchar_t* allocatorName, u$ bytes, const wchar_t* file, int line)
{
    korl_assert(bytes);
    const u$ heapBytes                 = 2 * _KORL_HEAP_SENTINEL_PADDING_BYTES + sizeof(*allocator);
    const u$ allocatableBytesTotal     = (allocator->virtualPages * korl_memory_pageBytes()) - heapBytes;
    const u$ allocatableBytesRemaining = allocatableBytesTotal - allocator->allocatedBytes;
    const u$ grossBytes                = sizeof(_Korl_Heap_Linear_AllocationMeta) + _KORL_HEAP_SENTINEL_PADDING_BYTES + bytes + _KORL_HEAP_SENTINEL_PADDING_BYTES;
    if(grossBytes > allocatableBytesRemaining)
    {
        if(!allocator->next)
        {
            korl_log(WARNING, "linear heap \"%ws\" out of memory", allocatorName);
            allocator->next = _korl_heap_linear_create(KORL_MATH_MAX(2 * allocator->virtualPages * korl_memory_pageBytes(), grossBytes + heapBytes), NULL);
        }
        return korl_heap_linear_allocate(allocator->next, allocatorName, bytes, file, line);
    }
    const u$ newHeapGrossBytes     = heapBytes + allocator->allocatedBytes + grossBytes;
    const u$ newHeapCommittedPages = korl_math_nextHighestDivision(newHeapGrossBytes, korl_memory_pageBytes());
    u8*const heapVirtualBase = KORL_C_CAST(u8*, allocator) - _KORL_HEAP_SENTINEL_PADDING_BYTES;
    if(newHeapCommittedPages > allocator->committedPages)
    {
        const u$ newCommittedPages = KORL_MATH_MIN(KORL_MATH_MAX(newHeapCommittedPages, 2 * allocator->committedPages), allocator->virtualPages);
        korl_log(VERBOSE, "expanding heap \"%ws\" pages: %llu => %llu", allocatorName, allocator->committedPages, newCommittedPages);
        const void*const resultCommit = VirtualAlloc(heapVirtualBase, newCommittedPages * korl_memory_pageBytes(), MEM_COMMIT, PAGE_READWRITE);
        if(!resultCommit)
            korl_logLastError("memory commit failed");
        allocator->committedPages = newCommittedPages;
    }
    _Korl_Heap_Linear_AllocationMeta*const resultMeta = KORL_C_CAST(_Korl_Heap_Linear_AllocationMeta*, heapVirtualBase + heapBytes + allocator->allocatedBytes);
    u8*const                               result     = heapVirtualBase + heapBytes + allocator->allocatedBytes + sizeof(*resultMeta) + _KORL_HEAP_SENTINEL_PADDING_BYTES;
    resultMeta->allocationMeta = KORL_STRUCT_INITIALIZE(Korl_Memory_AllocationMeta){.bytes = bytes, .file = file, .line = line};
    resultMeta->grossBytes     = grossBytes;
    korl_memory_set(resultMeta + 1, _KORL_HEAP_BYTE_PATTERN_SENTINEL, _KORL_HEAP_SENTINEL_PADDING_BYTES);
    korl_memory_zero(result, bytes);
    korl_memory_set(result + bytes, _KORL_HEAP_BYTE_PATTERN_SENTINEL, _KORL_HEAP_SENTINEL_PADDING_BYTES);
    allocator->allocatedBytes += grossBytes;
    return result;
}
korl_internal void* korl_heap_linear_reallocate(_Korl_Heap_Linear*const allocator, const wchar_t* allocatorName, void* allocation, u$ bytes, const wchar_t* file, int line)
{
    korl_assert(bytes);
    if(!allocation)
        return korl_heap_linear_allocate(allocator, allocatorName, bytes, file, line);
    const u$         heapBytes         = 2 * _KORL_HEAP_SENTINEL_PADDING_BYTES + sizeof(*allocator);
    u8*const         heapVirtualBase   = KORL_C_CAST(u8*, allocator) - _KORL_HEAP_SENTINEL_PADDING_BYTES;
    const void*const heapVirtualEnd    = heapVirtualBase + (allocator->virtualPages * korl_memory_pageBytes());
    const void*const heapAllocateBegin = heapVirtualBase + heapBytes;
    const void*const heapAllocateEnd   = heapVirtualBase + heapBytes + allocator->allocatedBytes;
    if(allocation < heapAllocateBegin || allocation >= heapAllocateEnd)
    {
        if(!allocator->next)
            korl_log(ERROR, "allocation 0x%X not found in allocator \"%ws\"", allocation, allocatorName);
        return korl_heap_linear_reallocate(allocator->next, allocatorName, allocation, bytes, file, line);
    }
    _Korl_Heap_Linear_AllocationMeta*const allocationMeta = KORL_C_CAST(_Korl_Heap_Linear_AllocationMeta*, KORL_C_CAST(u8*, allocation) - _KORL_HEAP_SENTINEL_PADDING_BYTES) - 1;
    if(/*we're in-place shrinking the allocation*/bytes <= allocationMeta->allocationMeta.bytes)
    {
        if(bytes < allocationMeta->allocationMeta.bytes)
        {
            korl_memory_set(KORL_C_CAST(u8*, allocation) + bytes, _KORL_HEAP_BYTE_PATTERN_FREE    , allocationMeta->allocationMeta.bytes - bytes);
            korl_memory_set(KORL_C_CAST(u8*, allocation) + bytes, _KORL_HEAP_BYTE_PATTERN_SENTINEL, _KORL_HEAP_SENTINEL_PADDING_BYTES);
        }
        /* if we're the last allocation, we should make sure the heap's allocatedBytes is up-to-date */
        if(/* we're the last allocation */KORL_C_CAST(u8*, allocationMeta) + allocationMeta->grossBytes >= KORL_C_CAST(const u8*, heapAllocateEnd))
        {
            korl_assert(KORL_C_CAST(u8*, allocationMeta) + allocationMeta->grossBytes == heapAllocateEnd);// the last allocation _must_ be aligned _exactly_ with the end of the heap's allocated byte region
            allocator->allocatedBytes  -= allocationMeta->allocationMeta.bytes - bytes;
            allocationMeta->grossBytes -= allocationMeta->allocationMeta.bytes - bytes;
        }
        allocator->isFragmented = true;
        allocationMeta->allocationMeta = KORL_STRUCT_INITIALIZE(Korl_Memory_AllocationMeta){.bytes = bytes, .file = file, .line = line};
        return allocation;
    }
    /* ----- everything below here _must_ involve _growing_ `allocation` ----- */
    const u$ maxBytes = allocationMeta->grossBytes - (sizeof(*allocationMeta) + 2 * _KORL_HEAP_SENTINEL_PADDING_BYTES);
    if(/*we're in-place expanding the allocation*/bytes <= maxBytes)
    {
        korl_memory_zero(KORL_C_CAST(u8*, allocation) + allocationMeta->allocationMeta.bytes, bytes - allocationMeta->allocationMeta.bytes);
        korl_memory_set(KORL_C_CAST(u8*, allocation) + bytes, _KORL_HEAP_BYTE_PATTERN_SENTINEL, _KORL_HEAP_SENTINEL_PADDING_BYTES);
        // we don't know enough information to know whether or not the allocator is still fragmented after this
        allocationMeta->allocationMeta = KORL_STRUCT_INITIALIZE(Korl_Memory_AllocationMeta){.bytes = bytes, .file = file, .line = line};
        return allocation;
    }
    const u$ grossBytesNew = sizeof(*allocationMeta) + _KORL_HEAP_SENTINEL_PADDING_BYTES + bytes + _KORL_HEAP_SENTINEL_PADDING_BYTES;
    if(   /* we're the last allocation */           KORL_C_CAST(u8*, allocationMeta) + allocationMeta->grossBytes >= KORL_C_CAST(const u8*, heapAllocateEnd)
       && /* grossBytesNew wont overflow the heap */KORL_C_CAST(u8*, allocationMeta) + grossBytesNew              <= KORL_C_CAST(const u8*, heapVirtualEnd))
    {
        korl_assert(KORL_C_CAST(u8*, allocationMeta) + allocationMeta->grossBytes == heapAllocateEnd);// the last allocation _must_ be aligned _exactly_ with the end of the heap's allocated byte region
        const u$ newHeapGrossBytes     = heapBytes + allocator->allocatedBytes - allocationMeta->grossBytes + grossBytesNew;
        const u$ newHeapCommittedPages = korl_math_nextHighestDivision(newHeapGrossBytes, korl_memory_pageBytes());
        if(newHeapCommittedPages > allocator->committedPages)
        {
            const u$ newCommittedPages = KORL_MATH_MIN(KORL_MATH_MAX(newHeapCommittedPages, 2 * allocator->committedPages), allocator->virtualPages);
            korl_log(VERBOSE, "expanding heap \"%ws\" pages: %llu => %llu", allocatorName, allocator->committedPages, newCommittedPages);
            const void*const resultCommit = VirtualAlloc(heapVirtualBase, newCommittedPages * korl_memory_pageBytes(), MEM_COMMIT, PAGE_READWRITE);
            if(!resultCommit)
                korl_logLastError("memory commit failed");
            allocator->committedPages = newCommittedPages;
        }
        korl_memory_zero(KORL_C_CAST(u8*, allocation) + allocationMeta->allocationMeta.bytes, bytes - allocationMeta->allocationMeta.bytes);
        korl_memory_set(KORL_C_CAST(u8*, allocation) + bytes, _KORL_HEAP_BYTE_PATTERN_SENTINEL, _KORL_HEAP_SENTINEL_PADDING_BYTES);
        allocator->allocatedBytes += grossBytesNew - allocationMeta->grossBytes;
        allocationMeta->allocationMeta = KORL_STRUCT_INITIALIZE(Korl_Memory_AllocationMeta){.bytes = bytes, .file = file, .line = line};
        allocationMeta->grossBytes     = grossBytesNew;
        return allocation;
    }
    /* we couldn't in-place shrink/expand; we must allocate => copy => free */
    korl_assert(bytes > allocationMeta->allocationMeta.bytes);
    void*const result = korl_heap_linear_allocate(allocator, allocatorName, bytes, file, line);
    korl_memory_copy(result, allocation, allocationMeta->allocationMeta.bytes);
    korl_heap_linear_free(allocator, allocation, file, line);
    return result;
}
korl_internal bool korl_heap_linear_isFragmented(_Korl_Heap_Linear*const allocator)
{
    return allocator->isFragmented;
}
/* sort support for linear heap addresses */
typedef struct _Korl_Heap_DefragmentPointer
{
    Korl_Heap_DefragmentPointer*   pDefragmentPointer;
    u32                            childrenSize;
    u32                            childrenCapacity;
     Korl_Heap_DefragmentPointer** children;
} _Korl_Heap_DefragmentPointer;
#ifndef SORT_CHECK_CAST_INT_TO_SIZET
    #define SORT_CHECK_CAST_INT_TO_SIZET(x) korl_checkCast_i$_to_u$(x)
#endif
#ifndef SORT_CHECK_CAST_SIZET_TO_INT
    #define SORT_CHECK_CAST_SIZET_TO_INT(x) korl_checkCast_u$_to_i32(x)
#endif
#define SORT_NAME _korl_heap_defragmentPointer_ascendHeapIndex_ascendAddress
#define SORT_TYPE _Korl_Heap_DefragmentPointer
#define SORT_CMP(x, y) (_korl_heap_linear_heapIndex(_korl_heap_defragmentPointer_getAllocation(*(x).pDefragmentPointer)) < _korl_heap_linear_heapIndex(_korl_heap_defragmentPointer_getAllocation(*(y).pDefragmentPointer)) ? -1 \
                        : _korl_heap_linear_heapIndex(_korl_heap_defragmentPointer_getAllocation(*(x).pDefragmentPointer)) > _korl_heap_linear_heapIndex(_korl_heap_defragmentPointer_getAllocation(*(y).pDefragmentPointer)) ? 1 \
                          : _korl_heap_defragmentPointer_getAllocation(*(x).pDefragmentPointer) < _korl_heap_defragmentPointer_getAllocation(*(y).pDefragmentPointer) ? -1 \
                            : _korl_heap_defragmentPointer_getAllocation(*(x).pDefragmentPointer) > _korl_heap_defragmentPointer_getAllocation(*(y).pDefragmentPointer) ? 1 \
                              : 0)
korl_global_variable _Korl_Heap_Linear* _korl_heap_defragmentPointer_sortContext;// @TODO: backlog this as a multi-threading hazard; in order to use uniform context with this sort API on multiple threads, we would need to use thread-local storage or something
korl_internal i32 _korl_heap_linear_heapIndex(const void*const allocation)
{
    i32                result = 0;
    _Korl_Heap_Linear* heap   = _korl_heap_defragmentPointer_sortContext;
    while(heap)
    {
        const u$         heapBytes         = 2 * _KORL_HEAP_SENTINEL_PADDING_BYTES + sizeof(*heap);
        u8*const         heapVirtualBase   = KORL_C_CAST(u8*, heap) - _KORL_HEAP_SENTINEL_PADDING_BYTES;
        const void*const heapAllocateBegin = heapVirtualBase + heapBytes;
        const void*const heapAllocateEnd   = heapVirtualBase + heapBytes + heap->allocatedBytes;
        if(allocation >= heapAllocateBegin && allocation < heapAllocateEnd)
            return result;
        result++;
        heap = heap->next;
    }
    return -1;
}
#include "sort.h"
#define SORT_NAME _korl_heap_defragmentPointer_ascendAddress
#define SORT_TYPE _Korl_Heap_DefragmentPointer
#define SORT_CMP(x, y) (_korl_heap_defragmentPointer_getAllocation(*(x).pDefragmentPointer) < _korl_heap_defragmentPointer_getAllocation(*(y).pDefragmentPointer) ? -1 \
                        : _korl_heap_defragmentPointer_getAllocation(*(x).pDefragmentPointer) > _korl_heap_defragmentPointer_getAllocation(*(y).pDefragmentPointer) ? 1 \
                          : 0)
#include "sort.h"
/**/
korl_internal void korl_heap_linear_defragment(_Korl_Heap_Linear*const allocator, const wchar_t* allocatorName, Korl_Heap_DefragmentPointer* defragmentPointers, u$ defragmentPointersSize, _Korl_Heap_Linear* stackAllocator, const wchar_t* stackAllocatorName, Korl_Memory_AllocatorHandle handleStackAllocator)
{
    korl_assert(defragmentPointersSize > 0);// the user _must_ pass a non-zero # of allocation pointers, otherwise this functionality would be impossible/pointless
    allocator->isFragmented = false;// assume we will, in fact, fully de-fragment the allocator; if we can't, we'll just raise this flag once more
    /* UH OH - we have another major issue; assume the heap looks like so:
            [FREE][allocA][allocB]
        and the user passes these defrag pointers:
            [&allocA][&allocB]
        if the struct stored in [allocA] contains [&allocB], during defragment:
            - [allocA] is moved into [FREE]
            - [&allocA] is correctly updated to the new address
            - _ERROR_(delayed); [&allocB] address has changed, but the defrag pointer is not updated!
            - [allocB] is moved into [allocA]'s trailing unused region
            - _ERROR_; [&allocB] defrag pointer update is attempted, but fails because it is pointing to gods-only-know where 
        the way I decided to mitigate this issue is by:
            - add a `parent` member to DefragmentPointer, which will allow the defragment algorithm to automatically update pointers that are stored in other allocations
            - require the user to pass a stack allocator so we can dynamically allocate temporary buffers which help mitigate the above issue; hopefully sacrificing some memory for speed */
    /* oh boy, yet another problem to solve; the user code for passing in DefragmentPointer parents is bad for several reasons:
        - the dynamic array favored by the user can't work, since the pointers will be invalidated as the user is accumulating the array
        - it's just annoying to keep track of which DefragmentPointer* is the parent of any given allocation; it's much easier to just select the void** parent
        so, it's up to us to take the slack of this luxury; allowing the user to just pass the void** parent, we must re-build the `_Korl_Heap_DefragmentPointer*`(child)=>`_Korl_Heap_DefragmentPointer*`(parent) map ourselves...
        I'm addressing this issue in the following way:
        - allocate a stb_ds hash-map and create a void**(child)=>DefragmentPointer*(parent) map
            fuck... there is a problem, since korl-stb-ds _only_ supports Korl_Memory_AllocatorHandle values for the memory context!
            for now, I will just modify this API to require the stack allocator's Korl_Memory_AllocatorHandle as well... */
    /* oh NO, yet another fucking problem; so we're in a situation here where a stb_ds struct (stbds_hash_index to be exact) 
        is storing pointers that point to addresses relative to the struct itself, just like how korl-gfx structs were before this 
        exact commit; to mitigate this issue, I am adding a stack-callback mechanism to DefragmentPointer, allowing the user to 
        perform custom actions on newly-moved allocations, such as manually offsetting a pointer to memory within the same allocation; 
        although honestly, going forward I am going to enforce a policy of _no_ allocation-local pointers stored in structs, in favor 
        of byte offsets which can be used to derive the pointers at run-time, at least for my own code */
    /* create a copy of defragmentPointers that we can shuffle, allowing the `parent` pointers to remain valid - size(n) - O(n) memory_copy*/
    _Korl_Heap_DefragmentPointer*const sortedDefragmentPointers = korl_heap_linear_allocate(stackAllocator, stackAllocatorName, defragmentPointersSize * sizeof(*defragmentPointers), __FILEW__, __LINE__);
    for(u$ i = 0; i < defragmentPointersSize; i++)
        sortedDefragmentPointers[i].pDefragmentPointer = &defragmentPointers[i];
    /* create a hash map which we can use to lookup void*(parentAllocation)=>Korl_Heap_DefragmentPointer*(parent) */
    struct {const void* key; _Korl_Heap_DefragmentPointer* value;}* stbHm_userAddress_to_defragPointer = NULL;
    mchmdefault(KORL_STB_DS_MC_CAST(handleStackAllocator), stbHm_userAddress_to_defragPointer, NULL);
    for(u$ i = 0; i < defragmentPointersSize; i++)
    {
        _Korl_Heap_DefragmentPointer*const defragmentPointer = sortedDefragmentPointers + i;
        mchmput(KORL_STB_DS_MC_CAST(handleStackAllocator), stbHm_userAddress_to_defragPointer, *defragmentPointer->pDefragmentPointer->userAddressPointer, defragmentPointer);
    }
    /* iterate over each DefragmentPointer, increment its parent's child count, and increment a `totalChildren` counter - O(n) */
    u32 totalChildren = 0;
    for(u$ i = 0; i < defragmentPointersSize; i++)
    {
        _Korl_Heap_DefragmentPointer*const defragmentPointer = sortedDefragmentPointers + i;
        if(!defragmentPointer->pDefragmentPointer->parent)
            continue;
        const i$ parentMapIndex = mchmgeti(KORL_STB_DS_MC_CAST(handleStackAllocator), stbHm_userAddress_to_defragPointer, defragmentPointer->pDefragmentPointer->parent);
        korl_assert(parentMapIndex >= 0);
        _Korl_Heap_DefragmentPointer*const parent = stbHm_userAddress_to_defragPointer[parentMapIndex].value;
        parent->childrenCapacity++;
        totalChildren++;
    }
    /* create a pool of Korl_Heap_DefragmentPointer* called `childPool` to store the child lists of each defrag pointer, of size `totalChildren` - size(<=n), as each DefragmentPointer can only have one parent */
    Korl_Heap_DefragmentPointer** childPool            = totalChildren ? korl_heap_linear_allocate(stackAllocator, stackAllocatorName, totalChildren * sizeof(*childPool), __FILEW__, __LINE__) : NULL;
    u$                            childPoolAllocations = 0;
    /* iterate over each DefragmentPointer, & add the defrag pointer to its parent's child list, assigning the parent's `childPool` offset if it doesn't already have one - O(n) */
    for(u$ i = 0; i < defragmentPointersSize; i++)
    {
        _Korl_Heap_DefragmentPointer*const defragmentPointer = sortedDefragmentPointers + i;
        if(!defragmentPointer->pDefragmentPointer->parent)
            continue;
        const i$ parentMapIndex = mchmgeti(KORL_STB_DS_MC_CAST(handleStackAllocator), stbHm_userAddress_to_defragPointer, defragmentPointer->pDefragmentPointer->parent);
        korl_assert(parentMapIndex >= 0);
        _Korl_Heap_DefragmentPointer*const parent = stbHm_userAddress_to_defragPointer[parentMapIndex].value;
        if(!parent->children)
        {
            parent->children = childPool + childPoolAllocations;
            childPoolAllocations += parent->childrenCapacity;
        }
        parent->children[parent->childrenSize++] = defragmentPointer->pDefragmentPointer;
    }
    /* sort the allocation pointer array by increasing heap-chain index, then by increasing address - O(nlogn) */
    if(allocator->next)
    {
        _korl_heap_defragmentPointer_sortContext = allocator;
        _korl_heap_defragmentPointer_ascendHeapIndex_ascendAddress_quick_sort(sortedDefragmentPointers, defragmentPointersSize);
    }
    else/* choose a less expensive sort if we know there is only one heap in the heap list */
        _korl_heap_defragmentPointer_ascendAddress_quick_sort(sortedDefragmentPointers, defragmentPointersSize);
    /* enumerate over each allocation - O(n), 
        remembering whether or not & how much trailing empty space is behind us; 
        if    the current allocation is the next allocationPointer 
           && the last non-free allocation was also an allocationPointer
           && there is a non-zero amount of unoccupied space between these two allocations, 
            move the allocation, such that there is no more space between these two allocations; 
            update the allocationPointer corresponding do this allocation appropriately; 
            make sure that the moved allocation occupies all bytes (as its [unused] component) */
    _Korl_Heap_Linear*                       currentHeap                = allocator;
    const u$                                 heapBytes                  = 2 * _KORL_HEAP_SENTINEL_PADDING_BYTES + sizeof(*allocator);
    const _Korl_Heap_DefragmentPointer*const defragmentPointersEnd      = sortedDefragmentPointers + defragmentPointersSize;
    const _Korl_Heap_DefragmentPointer*      defragmentPointersIterator = sortedDefragmentPointers;
    while(currentHeap)
    {
        const u8*const                    heapVirtualBase        = KORL_C_CAST(u8*, currentHeap) - _KORL_HEAP_SENTINEL_PADDING_BYTES;
        const void*const                  heapAllocateBegin      = heapVirtualBase + heapBytes;
        const void*                       heapAllocateEnd        = heapVirtualBase + heapBytes + currentHeap->allocatedBytes;
        _Korl_Heap_Linear_AllocationMeta* previousAllocationMeta = NULL;
        u$                                unoccupiedBytes        = 0;
        for(_Korl_Heap_Linear_AllocationMeta* allocationMeta = KORL_C_CAST(_Korl_Heap_Linear_AllocationMeta*, heapAllocateBegin)
           ;KORL_C_CAST(void*, allocationMeta) < heapAllocateEnd
           ;allocationMeta = KORL_C_CAST(_Korl_Heap_Linear_AllocationMeta*, KORL_C_CAST(u8*, allocationMeta) + allocationMeta->grossBytes))
        {
            void* allocation = KORL_C_CAST(u8*, allocationMeta) + sizeof(*allocationMeta) + _KORL_HEAP_SENTINEL_PADDING_BYTES;
            if(   defragmentPointersIterator < defragmentPointersEnd 
               && allocation == _korl_heap_defragmentPointer_getAllocation(*defragmentPointersIterator->pDefragmentPointer))// we can only move allocations whose address can be reported back to the user via the allocationPointerArray
            {
                korl_assert(allocationMeta->allocationMeta.bytes > 0);
                allocationMeta->allocationMeta.defragmentState = KORL_MEMORY_ALLOCATION_META_DEFRAGMENT_STATE_MANAGED;
                if(unoccupiedBytes)// we can only move allocations which are preceded by non-zero unoccupiedBytes
                {
                    /* before we copy/move the allocation back, we can update our grossBytes; this should have no effect on the proceding calculations */
                    allocationMeta->grossBytes += unoccupiedBytes;
                    /* move this entire allocation (excluding the [unused] component) into the start of the preceding unoccupied bytes */
                    const u$ allocationNetBytes = sizeof(*allocationMeta) + _KORL_HEAP_SENTINEL_PADDING_BYTES + allocationMeta->allocationMeta.bytes + _KORL_HEAP_SENTINEL_PADDING_BYTES;
                    if(unoccupiedBytes >= allocationNetBytes)
                    {
                        korl_memory_copy(KORL_C_CAST(u8*, allocationMeta) - unoccupiedBytes, allocationMeta, allocationNetBytes);
                        korl_memory_set(allocationMeta, _KORL_HEAP_BYTE_PATTERN_FREE, allocationNetBytes);
                    }
                    else
                    {
                        korl_memory_move(KORL_C_CAST(u8*, allocationMeta) - unoccupiedBytes, allocationMeta, allocationNetBytes);
                        korl_memory_set(KORL_C_CAST(u8*, allocationMeta) - unoccupiedBytes + allocationNetBytes, _KORL_HEAP_BYTE_PATTERN_FREE, unoccupiedBytes);
                    }
                    /* update the caller's allocation address, as well as our own pointers to allocation */
                    allocationMeta = KORL_C_CAST(_Korl_Heap_Linear_AllocationMeta*, KORL_C_CAST(u8*, allocationMeta) - unoccupiedBytes);
                    allocation     =                                                KORL_C_CAST(u8*, allocation)     - unoccupiedBytes;
                    _korl_heap_defragmentPointer_setAllocation(*defragmentPointersIterator->pDefragmentPointer, allocation);
                    /* if there is one, eliminate the previous allocation's unused region, as we will be occupying it if it exists */
                    if(previousAllocationMeta)
                        previousAllocationMeta->grossBytes = sizeof(*previousAllocationMeta) + _KORL_HEAP_SENTINEL_PADDING_BYTES + previousAllocationMeta->allocationMeta.bytes + _KORL_HEAP_SENTINEL_PADDING_BYTES;
                    /* when an allocation is moved, we iterate over all our child DefragmentPointer*s 
                        & shift their userAddressPointer by the same # of bytes
                        NOTE: we don't have to do this recursively! the only DefragmentPointers that become stale/invalid/dangling 
                              after we move an allocation are those who are _direct_ children of a defragmented allocation 
                              (dynamic allocation addresses that are stored within this allocation itself)! */
                    for(u$ i = 0; i < defragmentPointersIterator->childrenSize; i++)
                    {
                        /* this looks janky because it kinda is; we're literally offsetting the address of the child allocation pointer by a certain # of bytes, 
                            and the only way to do this is by casting the pointer to an integral datatype & operating on it, as far as I know... */
                        defragmentPointersIterator->children[i]->userAddressPointer = KORL_C_CAST(void**, KORL_C_CAST(u$, defragmentPointersIterator->children[i]->userAddressPointer) - unoccupiedBytes);
                        /* update our childrens' parent allocation address with the new one; 
                            this allows us to pass the correct parent address in the call to `onAllocationMovedCallback` below */
                        defragmentPointersIterator->children[i]->parent             = allocation;
                    }
                    /* if the user provided a callback function to this DefragmentPointer, we must call it now */
                    if(defragmentPointersIterator->pDefragmentPointer->onAllocationMovedCallback)
                        defragmentPointersIterator->pDefragmentPointer->onAllocationMovedCallback(defragmentPointersIterator->pDefragmentPointer->onAllocationMovedCallbackUserData
                                                                                                 ,allocation, allocationMeta->allocationMeta.bytes
                                                                                                 ,-korl_checkCast_u$_to_i$(unoccupiedBytes)
                                                                                                 ,defragmentPointersIterator->pDefragmentPointer->parent
                                                                                                 ,defragmentPointers, defragmentPointersSize);
                }
                defragmentPointersIterator++;
            }
            else
            {
                if(unoccupiedBytes)
                    allocator->isFragmented = true;
                allocationMeta->allocationMeta.defragmentState = KORL_MEMORY_ALLOCATION_META_DEFRAGMENT_STATE_UNMANAGED;
            }
            /* ensure that allocator->allocatedBytes contains _no_ trailing free allocations OR unused bytes */
            if(/* we're the last allocation */KORL_C_CAST(u8*, allocationMeta) + allocationMeta->grossBytes >= KORL_C_CAST(const u8*, heapAllocateEnd))
            {
                korl_assert(KORL_C_CAST(u8*, allocationMeta) + allocationMeta->grossBytes == heapAllocateEnd);// the last allocation _must_ be aligned _exactly_ with the end of the heap's allocated byte region)
                /* if the last allocation is free, we can collapse the heap's allocatedBytes region by the total size of the unoccupied bytes */
                if(allocationMeta->allocationMeta.bytes <= 0)
                {
                    currentHeap->allocatedBytes -= unoccupiedBytes + allocationMeta->grossBytes;             // this invalidates `heapAllocateEnd`
                    heapAllocateEnd              = heapVirtualBase + heapBytes + currentHeap->allocatedBytes;// so we have to re-calculate it here
                    /* if there is a previous non-free allocation, we _must_ eliminate its [unused] component, as it would now exceed the heap's allocatedBytes region */
                    if(previousAllocationMeta)
                        previousAllocationMeta->grossBytes = sizeof(*previousAllocationMeta) + _KORL_HEAP_SENTINEL_PADDING_BYTES + previousAllocationMeta->allocationMeta.bytes + _KORL_HEAP_SENTINEL_PADDING_BYTES;
                }
                else/* this allocation is being used; check if the allocation has a non-zero [unused] component*/
                {
                    const u$ unusedBytes = allocationMeta->grossBytes - (sizeof(*allocationMeta) + _KORL_HEAP_SENTINEL_PADDING_BYTES + allocationMeta->allocationMeta.bytes + _KORL_HEAP_SENTINEL_PADDING_BYTES);
                    if(unusedBytes)
                    {
                        /* if we have unusedBytes, since we know we are the last allocation, we shrink the heap's allocatedBytes region */
                        currentHeap->allocatedBytes -= unusedBytes;                                              // this invalidates `heapAllocateEnd`
                        heapAllocateEnd              = heapVirtualBase + heapBytes + currentHeap->allocatedBytes;// so we have to re-calculate it here
                        /* this allocation no longer has unusedBytes */
                        allocationMeta->grossBytes -= unusedBytes;
                    }
                }
            }
            if(allocationMeta->allocationMeta.bytes)
            {
                previousAllocationMeta = allocationMeta;// set the previous allocation to the last non-free allocation; this is the lower-limit which we can defragment the next non-free allocation
                /* reset unoccupied bytes to be this allocation's [unused] component */
                const u$ unusedBytes = allocationMeta->grossBytes - (sizeof(*allocationMeta) + _KORL_HEAP_SENTINEL_PADDING_BYTES + allocationMeta->allocationMeta.bytes + _KORL_HEAP_SENTINEL_PADDING_BYTES);
                unoccupiedBytes      = unusedBytes;
            }
            else
                /* accumulate all freed allocations as candidates for space we can move allocations into */
                unoccupiedBytes += allocationMeta->grossBytes;
        }
        currentHeap = currentHeap->next;
    }
    korl_assert(defragmentPointersIterator == defragmentPointersEnd);// ensure that we did, in fact, visit all allocationPointers in the array; if this fails, the user likely passed an allocation that doesn't exist in this heap list
}
korl_internal void korl_heap_linear_free(_Korl_Heap_Linear*const allocator, void* allocation, const wchar_t* file, int line)
{
    const u$         heapBytes         = 2 * _KORL_HEAP_SENTINEL_PADDING_BYTES + sizeof(*allocator);
    const u8*const   heapVirtualBase   = KORL_C_CAST(u8*, allocator) - _KORL_HEAP_SENTINEL_PADDING_BYTES;
    const void*const heapAllocateBegin = heapVirtualBase + heapBytes;
    const void*const heapAllocateEnd   = heapVirtualBase + heapBytes + allocator->allocatedBytes;
    if(allocation < heapAllocateBegin || allocation >= heapAllocateEnd)
    {
        if(!allocator->next)
            korl_log(ERROR, "allocation 0x%X not found", allocation);
        korl_heap_linear_free(allocator->next, allocation, file, line);
        return;
    }
    _Korl_Heap_Linear_AllocationMeta*const allocationMeta = KORL_C_CAST(_Korl_Heap_Linear_AllocationMeta*, KORL_C_CAST(u8*, allocation) - _KORL_HEAP_SENTINEL_PADDING_BYTES) - 1;
    korl_memory_set(allocation, _KORL_HEAP_BYTE_PATTERN_FREE, allocationMeta->allocationMeta.bytes);
    #if 0// do we actually want to know where the allocation was freed?...
    allocationMeta->allocationMeta.file  = file;
    allocationMeta->allocationMeta.line  = line;
    #endif
    allocationMeta->allocationMeta.bytes = 0;
    /* if we're the last allocation, we might as well update the allocator's 
        allocatedBytes metric to reduce unnecessary fragmentation; this should 
        be an insanely cheap operation, so no reason not to... */
    if(/* we're the last allocation */KORL_C_CAST(u8*, allocationMeta) + allocationMeta->grossBytes >= KORL_C_CAST(const u8*, heapAllocateEnd))
    {
        korl_assert(KORL_C_CAST(u8*, allocationMeta) + allocationMeta->grossBytes == heapAllocateEnd);// the last allocation _must_ be aligned _exactly_ with the end of the heap's allocated byte region)
        allocator->allocatedBytes -= allocationMeta->grossBytes;
    }
    allocator->isFragmented = true;
}
korl_internal KORL_HEAP_ENUMERATE(korl_heap_linear_enumerate)
{
    _Korl_Heap_Linear*const allocator         = heap;
    const u$                heapBytes         = 2 * _KORL_HEAP_SENTINEL_PADDING_BYTES + sizeof(*allocator);
    const u8*const          heapVirtualBase   = KORL_C_CAST(u8*, allocator) - _KORL_HEAP_SENTINEL_PADDING_BYTES;
    const void*const        heapVirtualEnd    = heapVirtualBase + allocator->virtualPages * korl_memory_pageBytes();
    callback(callbackUserData, heapVirtualBase, heapVirtualEnd, allocator->committedPages * korl_memory_pageBytes(), heapBytes + allocator->allocatedBytes);
    if(allocator->next)
        korl_heap_linear_enumerate(allocator->next, callback, callbackUserData);
}
korl_internal KORL_HEAP_ENUMERATE_ALLOCATIONS(korl_heap_linear_enumerateAllocations)
{
    _Korl_Heap_Linear* allocator = allocatorUserData;
    const u$           heapBytes = 2 * _KORL_HEAP_SENTINEL_PADDING_BYTES + sizeof(*allocator);
    while(allocator)
    {
        const u8*const   heapVirtualBase   = KORL_C_CAST(u8*, allocator) - _KORL_HEAP_SENTINEL_PADDING_BYTES;
        const void*const heapAllocateBegin = heapVirtualBase + heapBytes;
        const void*const heapAllocateEnd   = heapVirtualBase + heapBytes + allocator->allocatedBytes;
        for(_Korl_Heap_Linear_AllocationMeta* allocationMeta = KORL_C_CAST(_Korl_Heap_Linear_AllocationMeta*, heapAllocateBegin)
           ;KORL_C_CAST(void*, allocationMeta) < heapAllocateEnd
           ;allocationMeta = KORL_C_CAST(_Korl_Heap_Linear_AllocationMeta*, KORL_C_CAST(u8*, allocationMeta) + allocationMeta->grossBytes))
        {
            const u$ netBytes = sizeof(*allocationMeta) + _KORL_HEAP_SENTINEL_PADDING_BYTES + allocationMeta->allocationMeta.bytes + _KORL_HEAP_SENTINEL_PADDING_BYTES;
            if(!callback(callbackUserData
                        ,/*allocation*/KORL_C_CAST(u8*, allocationMeta) + sizeof(*allocationMeta) + _KORL_HEAP_SENTINEL_PADDING_BYTES
                        ,&allocationMeta->allocationMeta
                        ,allocationMeta->grossBytes
                        ,netBytes))
                return;/* stop enumerating allocations if the user's callback returns false */
        }
        allocator = allocator->next;
    }
}
korl_internal void korl_heap_linear_log(_Korl_Heap_Linear*const allocator, const wchar_t* allocatorName)
{
    korl_log(INFO, "===== heap \"%ws\" log =====", allocatorName);
    _Korl_Heap_Linear* currentHeap      = allocator;
    u$                 currentHeapIndex = 0;
    while(currentHeap)
    {
        const u$         heapBytes         = 2 * _KORL_HEAP_SENTINEL_PADDING_BYTES + sizeof(*currentHeap);
        const u8*const   heapVirtualBase   = KORL_C_CAST(u8*, currentHeap) - _KORL_HEAP_SENTINEL_PADDING_BYTES;
        const void*const heapVirtualEnd    = heapVirtualBase + currentHeap->virtualPages * korl_memory_pageBytes();
        const void*const heapAllocateBegin = heapVirtualBase + heapBytes;
        const void*const heapAllocateEnd   = heapVirtualBase + heapBytes + currentHeap->allocatedBytes;
        korl_log(INFO, "\t----- heap[%llu] -----", currentHeapIndex);
        korl_log(INFO, "\t\tvirtual address range: [0x%p, 0x%p), virtual bytes: %llu, committed bytes: %llu"
                ,heapVirtualBase, heapVirtualEnd, currentHeap->virtualPages * korl_memory_pageBytes()
                ,currentHeap->committedPages * korl_memory_pageBytes());
        korl_log(INFO, "\t\tallocated address range: [0x%p, 0x%p), allocated bytes: %llu"
                ,heapAllocateBegin, heapAllocateEnd, currentHeap->allocatedBytes);
        u$ currentAllocationIndex = 0;
        u$ fragmentedBytes        = 0;
        for(_Korl_Heap_Linear_AllocationMeta* allocationMeta = KORL_C_CAST(_Korl_Heap_Linear_AllocationMeta*, heapAllocateBegin)
           ;KORL_C_CAST(void*, allocationMeta) < heapAllocateEnd
           ; allocationMeta = KORL_C_CAST(_Korl_Heap_Linear_AllocationMeta*, KORL_C_CAST(u8*, allocationMeta) + allocationMeta->grossBytes)
            ,currentAllocationIndex++)
        {
            const u$ netBytes = sizeof(*allocationMeta) + _KORL_HEAP_SENTINEL_PADDING_BYTES + allocationMeta->allocationMeta.bytes + _KORL_HEAP_SENTINEL_PADDING_BYTES;
            korl_log(INFO, "\t\t- %hs allocation[%llu]: &meta=0x%p, &=0x%p, bytes=%llu, unusedBytes=%llu, grossBytes=%llu"
                    ,allocationMeta->allocationMeta.bytes ? "USED" : "FREE"
                    ,currentAllocationIndex
                    ,allocationMeta
                    ,/*allocation*/KORL_C_CAST(u8*, allocationMeta) + sizeof(*allocationMeta) + _KORL_HEAP_SENTINEL_PADDING_BYTES
                    ,allocationMeta->allocationMeta.bytes
                    ,allocationMeta->grossBytes - netBytes
                    ,allocationMeta->grossBytes);
            if(allocationMeta->allocationMeta.bytes)
                fragmentedBytes += allocationMeta->grossBytes - netBytes;
            else
                fragmentedBytes += allocationMeta->grossBytes;
        }
        korl_log(INFO, "\t\tfragmented bytes: %llu", fragmentedBytes);
        currentHeap = currentHeap->next;
        currentHeapIndex++;
    }
}
korl_internal void korl_heap_linear_debugUnitTests(void)
{
    korl_log(VERBOSE, "============= DEBUG UNIT TESTS");
    KORL_ZERO_STACK(Korl_Heap_CreateInfo, heapCreateInfo);
    heapCreateInfo.initialHeapBytes = 2 * korl_memory_pageBytes();
    korl_shared_const wchar_t DEBUG_HEAP_NAME[]       = L"DEBUG-linear-unit-test";
    korl_shared_const wchar_t DEBUG_HEAP_NAME_STACK[] = L"DEBUG-linear-unit-test-stack";
    KORL_MEMORY_POOL_DECLARE(u8*                        , allocations   , 32);
    KORL_MEMORY_POOL_DECLARE(Korl_Heap_DefragmentPointer, defragPointers, 32);
    KORL_MEMORY_POOL_SIZE(allocations) = 0;
    _Korl_Heap_Linear* heapStack = korl_heap_linear_create(&heapCreateInfo);
    _Korl_Heap_Linear* heap      = korl_heap_linear_create(&heapCreateInfo);
    Korl_Memory_AllocatorHandle allocatorHandleStack = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_LINEAR, L"DEBUG-linear-unit-test-stack2", KORL_MEMORY_ALLOCATOR_FLAG_EMPTY_EVERY_FRAME, &heapCreateInfo);
    korl_log(VERBOSE, "::::: create allocations :::::");
        *KORL_MEMORY_POOL_ADD(allocations) = korl_heap_linear_allocate(heap, DEBUG_HEAP_NAME, 32, __FILEW__, __LINE__);
        *KORL_MEMORY_POOL_ADD(allocations) = korl_heap_linear_allocate(heap, DEBUG_HEAP_NAME, 32, __FILEW__, __LINE__);
        korl_heap_linear_log(heap, DEBUG_HEAP_NAME);
    korl_log(VERBOSE, "::::: create partial internal fragmentation :::::");
        allocations[0] = korl_heap_linear_reallocate(heap, DEBUG_HEAP_NAME, allocations[0], 16, __FILEW__, __LINE__);
        korl_heap_linear_log(heap, DEBUG_HEAP_NAME);
    korl_log(VERBOSE, "::::: defragment :::::");
        KORL_MEMORY_POOL_RESIZE(defragPointers, KORL_MEMORY_POOL_SIZE(allocations));
        for(u$ i = 0; i < KORL_MEMORY_POOL_SIZE(allocations); i++)
            defragPointers[i] = (Korl_Heap_DefragmentPointer){&(allocations[i]), 0};
        korl_heap_linear_defragment(heap, DEBUG_HEAP_NAME, defragPointers, KORL_MEMORY_POOL_SIZE(defragPointers), heapStack, DEBUG_HEAP_NAME_STACK, allocatorHandleStack);
        korl_heap_linear_empty(heapStack);
        korl_heap_linear_log(heap, DEBUG_HEAP_NAME);
        for(u$ i = 0; i < KORL_MEMORY_POOL_SIZE(allocations); i++)
            korl_log(INFO, "allocations[%llu]: &=0x%p", i, allocations[i]);
    korl_log(VERBOSE, "::::: create full internal fragmentation :::::");
        korl_heap_linear_free(heap, allocations[0], __FILEW__, __LINE__);
        KORL_MEMORY_POOL_REMOVE(allocations, 0);
        korl_heap_linear_log(heap, DEBUG_HEAP_NAME);
    korl_log(VERBOSE, "::::: defragment :::::");
        KORL_MEMORY_POOL_RESIZE(defragPointers, KORL_MEMORY_POOL_SIZE(allocations));
        for(u$ i = 0; i < KORL_MEMORY_POOL_SIZE(allocations); i++)
            defragPointers[i] = (Korl_Heap_DefragmentPointer){&(allocations[i]), 0};
        korl_heap_linear_defragment(heap, DEBUG_HEAP_NAME, defragPointers, KORL_MEMORY_POOL_SIZE(defragPointers), heapStack, DEBUG_HEAP_NAME_STACK, allocatorHandleStack);
        korl_heap_linear_empty(heapStack);
        korl_heap_linear_log(heap, DEBUG_HEAP_NAME);
        for(u$ i = 0; i < KORL_MEMORY_POOL_SIZE(allocations); i++)
            korl_log(INFO, "allocations[%llu]: &=0x%p", i, allocations[i]);
    korl_log(VERBOSE, "::::: attempt to create partial trailing fragmentation (this should not cause fragmentation) :::::");
        *KORL_MEMORY_POOL_ADD(allocations) = korl_heap_linear_allocate(heap, DEBUG_HEAP_NAME, 32, __FILEW__, __LINE__);
        allocations[1] = korl_heap_linear_reallocate(heap, DEBUG_HEAP_NAME, allocations[1], 16, __FILEW__, __LINE__);
        *KORL_MEMORY_POOL_ADD(allocations) = korl_heap_linear_allocate(heap, DEBUG_HEAP_NAME, 32, __FILEW__, __LINE__);
        korl_heap_linear_log(heap, DEBUG_HEAP_NAME);
    korl_log(VERBOSE, "::::: attempt to create full trailing fragmentation (this should not cause fragmentation) :::::");
        korl_heap_linear_free(heap, KORL_MEMORY_POOL_POP(allocations), __FILEW__, __LINE__);
        korl_heap_linear_log(heap, DEBUG_HEAP_NAME);
    korl_log(VERBOSE, "::::: create partial mid fragmentation :::::");
        allocations[KORL_MEMORY_POOL_SIZE(allocations) - 1] = korl_heap_linear_reallocate(heap, DEBUG_HEAP_NAME, KORL_MEMORY_POOL_LAST(allocations), 32, __FILEW__, __LINE__);
        *KORL_MEMORY_POOL_ADD(allocations) = korl_heap_linear_allocate(heap, DEBUG_HEAP_NAME, 32, __FILEW__, __LINE__);
        allocations[1] = korl_heap_linear_reallocate(heap, DEBUG_HEAP_NAME, allocations[1], 16, __FILEW__, __LINE__);
        korl_heap_linear_log(heap, DEBUG_HEAP_NAME);
    korl_log(VERBOSE, "::::: defragment :::::");
        KORL_MEMORY_POOL_RESIZE(defragPointers, KORL_MEMORY_POOL_SIZE(allocations));
        for(u$ i = 0; i < KORL_MEMORY_POOL_SIZE(allocations); i++)
            defragPointers[i] = (Korl_Heap_DefragmentPointer){&(allocations[i]), 0};
        korl_heap_linear_defragment(heap, DEBUG_HEAP_NAME, defragPointers, KORL_MEMORY_POOL_SIZE(defragPointers), heapStack, DEBUG_HEAP_NAME_STACK, allocatorHandleStack);
        korl_heap_linear_empty(heapStack);
        korl_heap_linear_log(heap, DEBUG_HEAP_NAME);
        for(u$ i = 0; i < KORL_MEMORY_POOL_SIZE(allocations); i++)
            korl_log(INFO, "allocations[%llu]: &=0x%p", i, allocations[i]);
    korl_log(VERBOSE, "::::: create full mid fragmentation :::::");
        korl_heap_linear_free(heap, allocations[1], __FILEW__, __LINE__);
        KORL_MEMORY_POOL_REMOVE(allocations, 1);
        korl_heap_linear_log(heap, DEBUG_HEAP_NAME);
    korl_log(VERBOSE, "::::: defragment :::::");
        KORL_MEMORY_POOL_RESIZE(defragPointers, KORL_MEMORY_POOL_SIZE(allocations));
        for(u$ i = 0; i < KORL_MEMORY_POOL_SIZE(allocations); i++)
            defragPointers[i] = (Korl_Heap_DefragmentPointer){&(allocations[i]), 0};
        korl_heap_linear_defragment(heap, DEBUG_HEAP_NAME, defragPointers, KORL_MEMORY_POOL_SIZE(defragPointers), heapStack, DEBUG_HEAP_NAME_STACK, allocatorHandleStack);
        korl_heap_linear_empty(heapStack);
        korl_heap_linear_log(heap, DEBUG_HEAP_NAME);
        for(u$ i = 0; i < KORL_MEMORY_POOL_SIZE(allocations); i++)
            korl_log(INFO, "allocations[%llu]: &=0x%p", i, allocations[i]);
    korl_log(VERBOSE, "::::: create pseudo-stb_ds-array :::::");
        *KORL_MEMORY_POOL_ADD(allocations) = korl_heap_linear_allocate(heap, DEBUG_HEAP_NAME, 32, __FILEW__, __LINE__);
        *allocations[KORL_MEMORY_POOL_SIZE(allocations) - 1] = 3;// dynamic array size = 3
        allocations[KORL_MEMORY_POOL_SIZE(allocations) - 1] += 1;// header is 1 byte; advance the allocation to the array payload
        for(u8 i = 0; i < 3; i++)
            allocations[KORL_MEMORY_POOL_SIZE(allocations) - 1][i] = i;// initialize our array values
        korl_heap_linear_log(heap, DEBUG_HEAP_NAME);
        for(u8 i = 0; i < 3; i++)
            korl_log(INFO, "testDynamicArray[%hhu]==%hhu", i, allocations[KORL_MEMORY_POOL_SIZE(allocations) - 1][i]);
    korl_log(VERBOSE, "::::: create full mid fragmentation :::::");
        korl_heap_linear_free(heap, allocations[1], __FILEW__, __LINE__);
        KORL_MEMORY_POOL_REMOVE(allocations, 1);
        korl_heap_linear_log(heap, DEBUG_HEAP_NAME);
    korl_log(VERBOSE, "::::: defragment :::::");
        KORL_MEMORY_POOL_RESIZE(defragPointers, KORL_MEMORY_POOL_SIZE(allocations));
        for(u$ i = 0; i < KORL_MEMORY_POOL_SIZE(allocations); i++)
            defragPointers[i] = (Korl_Heap_DefragmentPointer){&(allocations[i]), 0};
        defragPointers[1].userAddressByteOffset = -1;// dynamic array header is the _true_ allocation address
        korl_heap_linear_defragment(heap, DEBUG_HEAP_NAME, defragPointers, KORL_MEMORY_POOL_SIZE(defragPointers), heapStack, DEBUG_HEAP_NAME_STACK, allocatorHandleStack);
        korl_heap_linear_empty(heapStack);
        korl_heap_linear_log(heap, DEBUG_HEAP_NAME);
        for(u$ i = 0; i < KORL_MEMORY_POOL_SIZE(allocations); i++)
            korl_log(INFO, "allocations[%llu]: &=0x%p", i, allocations[i]);
        for(u8 i = 0; i < 3; i++)
            korl_log(INFO, "testDynamicArray[%hhu]==%hhu", i, allocations[1][i]);// we should still be able to use our dynamic array as normal
    korl_heap_linear_destroy(heap);
    KORL_MEMORY_POOL_EMPTY(allocations);
    KORL_MEMORY_POOL_EMPTY(defragPointers);
    heap = korl_heap_linear_create(&heapCreateInfo);
    korl_log(VERBOSE, "::::: create scenario where a dynamic struct holds a pointer to another dynamic struct :::::");
        typedef struct _LinkedList
        {
            u32 data;
            struct _LinkedList* next;
        } _LinkedList;
        _LinkedList* nodes[2];
        nodes[0] = korl_heap_linear_allocate(heap, DEBUG_HEAP_NAME, sizeof(_LinkedList), __FILEW__, __LINE__);
        nodes[1] = korl_heap_linear_allocate(heap, DEBUG_HEAP_NAME, sizeof(_LinkedList), __FILEW__, __LINE__);
        nodes[1]->data = 0x69420;
        nodes[1]->next = korl_heap_linear_allocate(heap, DEBUG_HEAP_NAME, sizeof(_LinkedList), __FILEW__, __LINE__);
        nodes[1]->next->data = 0xDEADFEE7;
        korl_heap_linear_log(heap, DEBUG_HEAP_NAME);
        for(u$ i = 0; i < korl_arraySize(nodes); i++)
            for(const _LinkedList* node = nodes[i]; node; node = node->next)
                if(node == nodes[i])
                    korl_log(INFO, "nodes[%llu]: &=0x%p data=0x%X next=0x%p", i, node, node->data, node->next);
                else
                    korl_log(INFO, "\tchild: &=0x%p data=0x%X next=0x%p", node, node->data, node->next);
    korl_log(VERBOSE, "::::: delete node[0], then defragment :::::");
        korl_heap_linear_free(heap, nodes[0], __FILEW__, __LINE__); nodes[0] = NULL;
        KORL_MEMORY_POOL_EMPTY(defragPointers);
        *KORL_MEMORY_POOL_ADD(defragPointers) = (Korl_Heap_DefragmentPointer){&nodes[1], 0, NULL};
        *KORL_MEMORY_POOL_ADD(defragPointers) = (Korl_Heap_DefragmentPointer){&(nodes[1]->next), 0, &nodes[1]};
        korl_heap_linear_defragment(heap, DEBUG_HEAP_NAME, defragPointers, KORL_MEMORY_POOL_SIZE(defragPointers), heapStack, DEBUG_HEAP_NAME_STACK, allocatorHandleStack);
        korl_heap_linear_empty(heapStack);
        korl_heap_linear_log(heap, DEBUG_HEAP_NAME);
        for(u$ i = 0; i < korl_arraySize(nodes); i++)
            for(const _LinkedList* node = nodes[i]; node; node = node->next)
                if(node == nodes[i])
                    korl_log(INFO, "nodes[%llu]: &=0x%p data=0x%X next=0x%p", i, node, node->data, node->next);
                else
                    korl_log(INFO, "\tchild: &=0x%p data=0x%X next=0x%p", node, node->data, node->next);
        korl_assert(nodes[1]->data == 0x69420);
        korl_assert(nodes[1]->next->data == 0xDEADFEE7);
    korl_heap_linear_destroy(heap);
    korl_memory_allocator_destroy(allocatorHandleStack);
    korl_log(VERBOSE, "END DEBUG UNIT TESTS ===============");
}
