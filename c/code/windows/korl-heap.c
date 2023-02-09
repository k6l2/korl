#include "korl-heap.h"
#include "korl-windows-globalDefines.h"
#include "korl-interface-platform.h"// for KorlEnumLogLevel
#define _KORL_HEAP_INVALID_BYTE_PATTERN 0xFE
#define _KORL_HEAP_GENERAL_GUARD_UNUSED_ALLOCATION_PAGES 1
#define _KORL_HEAP_GENERAL_GUARD_ALLOCATOR 1
#define _KORL_HEAP_GENERAL_LAZY_COMMIT_PAGES 1
#define _KORL_HEAP_GENERAL_DEBUG 0
korl_global_const i8 _KORL_HEAP_GENERAL_ALLOCATION_META_SEPARATOR[]     = "[KORL-general-allocation]";
korl_global_const i8 _KORL_HEAP_LINEAR_ALLOCATION_META_SEPARATOR[]      = "[KORL-linear-allocation]";
korl_global_const i8 _KORL_HEAP_LINEAR_ALLOCATION_META_SEPARATOR_FREE[] = "[KORL-linear-free-alloc]";
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
    u$ availablePageFlags[];// see comments about this data structure for details of how this is used; the size of this array is determined by `bytes`, which should be calculated as `nextHighestDivision(nextHighestDivision(bytes, pageBytes), bitCount(u$))`
} _Korl_Heap_General;
typedef struct _Korl_Heap_General_AllocationMeta
{
    Korl_Memory_AllocationMeta allocationMeta;
    u$ pagesCommitted;// Overengineering; all this can do is allow us to support reserving of arbitrary # of pages.  In reality, I probably will never implement this, and if so, the page count can be derived from calculating `nextHighestDivision(sizeof(_Korl_Heap_General_AllocationMeta) + metaData.bytes, pageBytes)`
} _Korl_Heap_General_AllocationMeta;
/* The main purpose of the Linear Allocator is to have an extremely light & fast 
    allocation strategy.  We want to sacrifice tons of memory for the sake of 
    speed.  Desired behavior characteristics:
    - _always_ take new memory from the end of the allocator, _unless_ the 
      allocator is emptied (lastAllocation == NULL)
    - retain the ability to iterate over all allocations
    - when an allocation is freed, it will essentially cause the allocation 
      before it to "realloc" into the space it occupies
    - the exception is the very first allocation, which has no allocations 
      "before" it in memory
    Thus effectively creating a double-linked list, where the "previous" 
    node is tracked via a pointer, and the "next" node is implicit since we 
    can assume that the allocator is fully packed, meaning there is no empty 
    space between allocations.  Ergo, we know the next node is at 
    `allocation + allocation.bytes` in memory!
    We must be willing to accept the following consequences for the sake of 
    speed/simplicity!!!:
    - the main downside to this strategy is that we set ourselves up to leaking 
      infinite memory if we are continuously allocating things and never 
      emptying the allocator!  If we continously allocate memory without ever 
      emptying the allocator, we will eventually run out of memory very quickly
    - reallocating memory alternately between multiple allocations will yield 
      extremely poor performance in both time/space, since the allocations will 
      likely need to be copied each time to the end of the allocator, and the 
      unused space will never be recycled (see previous consequence)
    */
typedef struct _Korl_Heap_Linear
{
    /** total amount of reserved virtual address space, including this struct 
     * calculated as: korl_math_nextHighestDivision(maxBytes, pageBytes) * 
    */
    u$ bytes;//KORL-ISSUE-000-000-030: redundant: use QueryVirtualMemoryInformation instead
    void* lastAllocation;
    _Korl_Heap_Linear* next;// support for "limitless" heap; if this allocator fails to allocate, we shunt to the `next` heap, or create a new heap if `next` doesn't exist yet
} _Korl_Heap_Linear;
typedef struct _Korl_Heap_Linear_AllocationMeta
{
    Korl_Memory_AllocationMeta allocationMeta;
    void* previousAllocation;
    u$ availableBytes;// calculated as: (metaBytesRequired + bytes) rounded up to nearest page size
} _Korl_Heap_Linear_AllocationMeta;
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
                const u$ mask = precedingBits < bitsPerRegister // overflow left shift is undefined behavior in C, as I just learned!
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
                const u$ mask  = flags < bitsPerRegister // overflow left shift is undefined behavior in C, as I just learned!
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
korl_internal void _korl_heap_linear_allocatorPageGuard(_Korl_Heap_Linear* allocator)
{
    DWORD oldProtect;
    if(!VirtualProtect(allocator, sizeof(*allocator), PAGE_READWRITE | PAGE_GUARD, &oldProtect))
        korl_logLastError("VirtualProtect failed!");
    korl_assert(oldProtect == PAGE_READWRITE);
}
korl_internal void _korl_heap_linear_allocatorPageUnguard(_Korl_Heap_Linear* allocator)
{
    DWORD oldProtect;
    if(!VirtualProtect(allocator, sizeof(*allocator), PAGE_READWRITE, &oldProtect))
        korl_logLastError("VirtualProtect failed!");
    korl_assert(oldProtect == (PAGE_READWRITE | PAGE_GUARD));
}

/** iterate over allocations recursively on the stack so that we can iterate in 
 * monotonically-increasing memory addresses */
korl_internal bool _korl_heap_linear_enumerateAllocationsRecurse(_Korl_Heap_Linear*const allocator, void* allocation, fnSig_korl_memory_allocator_enumerateAllocationsCallback* callback, void* callbackUserData)
{
    /* halt recursion when we reach a NULL allocation */
    if(!allocation)
        return false;
    /* sanity checks */
    const u$ pageBytes = korl_memory_pageBytes();
    /* ensure that this address is actually within the allocator's range */
    const u$ allocatorPages = korl_math_nextHighestDivision(sizeof(*allocator), pageBytes);
    korl_assert(KORL_C_CAST(u8*, allocation) >= KORL_C_CAST(u8*, allocator) + (allocatorPages * pageBytes)
             && KORL_C_CAST(u8*, allocation) <  KORL_C_CAST(u8*, allocator) + allocator->bytes);
    /* ensure that the allocation meta address is divisible by the page size, 
        because this is currently a constraint of this allocator */
    const u$ metaBytesRequired = sizeof(_Korl_Heap_Linear_AllocationMeta) 
                               + sizeof(_KORL_HEAP_LINEAR_ALLOCATION_META_SEPARATOR) - 1/*exclude null terminator*/;
    korl_assert((KORL_C_CAST(u$, allocation) - metaBytesRequired) % pageBytes == 0);
    /* ensure that the memory just before the allocation is the constant meta 
        separator string */
    korl_assert(0 == korl_memory_compare(KORL_C_CAST(u8*, allocation) - (sizeof(_KORL_HEAP_LINEAR_ALLOCATION_META_SEPARATOR) - 1/*exclude null terminator*/), 
                                         _KORL_HEAP_LINEAR_ALLOCATION_META_SEPARATOR, 
                                         sizeof(_KORL_HEAP_LINEAR_ALLOCATION_META_SEPARATOR) - 1/*exclude null terminator*/));
    /* recurse into the previous allocation */
    _Korl_Heap_Linear_AllocationMeta*const allocationMeta = 
        KORL_C_CAST(_Korl_Heap_Linear_AllocationMeta*, KORL_C_CAST(u8*, allocation) - metaBytesRequired);
    const bool resultRecurse = _korl_heap_linear_enumerateAllocationsRecurse(allocator, allocationMeta->previousAllocation, callback, callbackUserData);
    /* perform the enumeration callback after the stack is drained */
    //KORL-ISSUE-000-000-080: memory: korl_heap_linear_enumerateAllocations can't properly early-out if the enumeration callback returns false
    return callback(callbackUserData, allocation, &(allocationMeta->allocationMeta)) | resultRecurse;
}
korl_internal _Korl_Heap_General* korl_heap_general_create(u$ maxBytes, void* address)
{
    korl_assert(maxBytes > 0);
    _Korl_Heap_General* result = NULL;
    const u$ pageBytes                     = korl_memory_pageBytes();
    const u$ pagesRequired                 = korl_math_nextHighestDivision(maxBytes, pageBytes);
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
korl_internal void* korl_heap_general_allocate(_Korl_Heap_General* allocator, u$ bytes, const wchar_t* file, int line, void* requestedAddress)
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
    if(requestedAddress)
    {
        _Korl_Heap_General_AllocationMeta*const requestedMeta = KORL_C_CAST(_Korl_Heap_General_AllocationMeta*, 
            KORL_C_CAST(u8*, requestedAddress) - metaBytesRequired);
        korl_assert(KORL_C_CAST(u$, requestedMeta) % pageBytes == 0);
        const void*const allocationRegionBegin = KORL_C_CAST(u8*, allocator) + allocatorPages*pageBytes;
        const void*const allocationRegionEnd   = KORL_C_CAST(u8*, allocationRegionBegin) + allocator->allocationPages*pageBytes;
        if(requestedAddress < allocationRegionBegin || requestedAddress >= allocationRegionEnd)
        {
            /* if the user requested an address, and that address is outside of this 
                heap's virtual memory range, we need to check the next heap in the 
                linked list to see if it can contain the address */
            korl_assert(!"@TODO");
        }
        korl_assert(requestedAddress >= allocationRegionBegin);// we must be inside the allocation region
        korl_assert(requestedAddress <  allocationRegionEnd);  // we must be inside the allocation region
        const u$ requestedAllocationPage = (KORL_C_CAST(u$, requestedMeta) - KORL_C_CAST(u$, allocationRegionBegin)) / pageBytes;
        /* ensure that the page flags required are not occupied */
        const u$ highestOccupiedPageOffset = _korl_heap_general_highestOccupiedPageOffset(allocator, requestedAllocationPage - 1/*account for preceding guard page*/, allocationPages + 1/*include preceding guard page*/);
        if(highestOccupiedPageOffset < allocationPages + 1/*include preceding guard page*/)
        {
            korl_log(WARNING, "Allocation failure; requested address is occupied!");
            goto guardAllocator_returnResult;
        }
        /* set the available page index appropriately & occupy the necessary page flags */
        _korl_heap_general_setPageFlags(allocator, requestedAllocationPage, allocationPages, true/*occupied*/);
        availablePageIndex = requestedAllocationPage;
    }
    else
        availablePageIndex = _korl_heap_general_occupyAvailablePages(allocator, allocationPages);
    if(   availablePageIndex                   >= allocator->allocationPages 
       || availablePageIndex + allocationPages >  allocator->allocationPages/*required only because of KORL-ISSUE-000-000-088*/)
    {
        korl_log(WARNING, "occupyAvailablePages failed; allocator may require defragmentation, or may have run out of space");
        if(availablePageIndex + allocationPages > allocator->allocationPages)
            _korl_heap_general_setPageFlags(allocator, availablePageIndex, allocationPages, false);//required only because of KORL-ISSUE-000-000-088
        if(!allocator->next)
        {
            /* we want to either (depending on which results in more pages): 
                - double the amount of allocationPages of the next allocator
                - have at _least_ the # of allocationPages required to satisfy `bytes` */
            const u$ nextHeapPages = KORL_MATH_MAX(2*allocator->allocationPages, 1/*include preceding guard page*/+allocationPages);
            allocator->next = korl_heap_general_create((allocatorPages + nextHeapPages)*pageBytes, NULL);
        }
        result = korl_heap_general_allocate(allocator->next, bytes, file, line, requestedAddress);
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
korl_internal void* korl_heap_general_reallocate(_Korl_Heap_General* allocator, void* allocation, u$ bytes, const wchar_t* file, int line)
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
        allocation = korl_heap_general_reallocate(allocator->next, allocation, bytes, file, line);
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
        korl_log(WARNING, "occupyAvailablePages failed; allocator may require defragmentation, or may have run out of space");
        if(newAllocationPage + newAllocationPages > allocator->allocationPages)
            _korl_heap_general_setPageFlags(allocator, newAllocationPage, newAllocationPages, false);//required only because of KORL-ISSUE-000-000-088
        // create a new allocation in another heap in the heap linked list //
        if(!allocator->next)
        {
            /* we want to either (depending on which results in more pages): 
                - double the amount of allocationPages of the next allocator
                - have at _least_ the # of allocationPages required to satisfy `bytes` */
            const u$ nextHeapPages = KORL_MATH_MAX(2*allocator->allocationPages, 1/*include preceding guard page*/+newAllocationPages);
            allocator->next = korl_heap_general_create((allocatorPages + nextHeapPages)*pageBytes, NULL);
        }
        void*const newAllocation = korl_heap_general_allocate(allocator->next, bytes, file, line, NULL);
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
    callback(callbackUserData, allocator, virtualAddressEnd);
    if(allocator->next)
        korl_heap_general_enumerate(callback, callbackUserData, allocator->next);
    _korl_heap_general_allocatorPagesGuard(allocator);
}
korl_internal KORL_MEMORY_ALLOCATOR_ENUMERATE_ALLOCATIONS(korl_heap_general_enumerateAllocations)
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
            const u$ pageFlagsRemainderInRegisterMask = pageFlagsRemainderInRegister < bitsPerFlagRegister // overflow left shift is undefined behavior
                ? ~((~0LLU) << pageFlagsRemainderInRegister) 
                : ~0LLU;
            pageFlagRegister &= ~(pageFlagsRemainderInRegisterMask << (bitsPerFlagRegister - pageFlagsRemainderInRegister));
            pageFlagsRemainder -= pageFlagsRemainderInRegister;
            /**/
            unsigned long mostSignificantSetBitIndex = korl_checkCast_u$_to_u32(bitsPerFlagRegister)/*smallest invalid value*/;
            while(_BitScanReverse64(&mostSignificantSetBitIndex, pageFlagRegister))
            {
                const u$ allocationPageIndex = pfr*bitsPerFlagRegister + (bitsPerFlagRegister - 1 - mostSignificantSetBitIndex);
                const _Korl_Heap_General_AllocationMeta*const metaAddress = KORL_C_CAST(_Korl_Heap_General_AllocationMeta*, 
                    KORL_C_CAST(u8*, allocator) + (allocatorPages + allocationPageIndex)*pageBytes);
                const u$ metaBytesRequired = sizeof(*metaAddress) + sizeof(_KORL_HEAP_GENERAL_ALLOCATION_META_SEPARATOR) - 1/*don't include null-terminator*/;
                if(!callback(callbackUserData, KORL_C_CAST(u8*, metaAddress) + metaBytesRequired, &(metaAddress->allocationMeta)))
                {
                    abortEnumeration = true;
                    goto guardAllocator;
                }
                const u$ maxFlagsInRegister        = mostSignificantSetBitIndex + 1;
                const u$ allocationFlagsInRegister = KORL_MATH_MIN(maxFlagsInRegister, metaAddress->pagesCommitted);
                /* remove the flags that this allocation occupies */
                const u$ allocationFlagsInRegisterMask = ~((~0LLU) << allocationFlagsInRegister);
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
korl_internal _Korl_Heap_Linear* korl_heap_linear_create(u$ maxBytes, void* address)
{
    _Korl_Heap_Linear* result = NULL;
    korl_assert(maxBytes > 0);
    korl_assert(maxBytes >= sizeof(*result));
    const u$ pagesRequired      = korl_math_nextHighestDivision(maxBytes, korl_memory_pageBytes());
    const u$ totalBytesRequired = pagesRequired * korl_memory_pageBytes();
    /* reserve all these pages */
    LPVOID resultVirtualAlloc = VirtualAlloc(address, totalBytesRequired, MEM_RESERVE, PAGE_NOACCESS);
    if(resultVirtualAlloc == NULL)
        korl_logLastError("VirtualAlloc failed!");
    result = resultVirtualAlloc;
    /* commit the first pages for the allocator itself */
    const u$ allocatorPages = korl_math_nextHighestDivision(sizeof(*result), korl_memory_pageBytes());
    const u$ allocatorBytes = allocatorPages * korl_memory_pageBytes();
    resultVirtualAlloc = VirtualAlloc(result, allocatorBytes, MEM_COMMIT, PAGE_READWRITE);
    if(resultVirtualAlloc == NULL)
        korl_logLastError("VirtualAlloc failed!");
    /* initialize the memory of the allocator userData */
    korl_shared_const i8 ALLOCATOR_PAGE_MEMORY_PATTERN[] = "KORL-LinearAllocator-Page|";
    _Korl_Heap_Linear*const allocator = KORL_C_CAST(_Korl_Heap_Linear*, resultVirtualAlloc);
    // fill the allocator page with a readable pattern for easier debugging //
    for(u$ currFillByte = 0; currFillByte < allocatorBytes; )
    {
        const u$ remainingBytes = allocatorBytes - currFillByte;
        const u$ availableFillBytes = KORL_MATH_MIN(remainingBytes, sizeof(ALLOCATOR_PAGE_MEMORY_PATTERN) - 1/*do not copy NULL-terminator*/);
        korl_memory_copy(KORL_C_CAST(u8*, allocator) + currFillByte, ALLOCATOR_PAGE_MEMORY_PATTERN, availableFillBytes);
        currFillByte += availableFillBytes;
    }
    korl_memory_zero(allocator, sizeof(*allocator));
    allocator->bytes = totalBytesRequired;
    /* guard the allocator page */
    _korl_heap_linear_allocatorPageGuard(allocator);
    return result;
}
korl_internal void korl_heap_linear_destroy(_Korl_Heap_Linear*const allocator)
{
    _korl_heap_linear_allocatorPageUnguard(allocator);
    if(allocator->next)
        korl_heap_linear_destroy(allocator->next);
    if(!VirtualFree(allocator, 0/*release everything*/, MEM_RELEASE))
        korl_logLastError("VirtualFree failed!");
}
korl_internal void korl_heap_linear_empty(_Korl_Heap_Linear*const allocator)
{
    /* sanity checks */
    _korl_heap_linear_allocatorPageUnguard(allocator);
    if(allocator->next)
        korl_heap_linear_empty(allocator->next);
    const u$ pageBytes = korl_memory_pageBytes();
    korl_assert(0 == KORL_C_CAST(u$, allocator) % korl_memory_virtualAlignBytes());
    korl_assert(0 == allocator->bytes           % pageBytes);
    /* eliminate all allocations */
    /* MSDN page for VirtualFree says: 
        "The function does not fail if you attempt to decommit an uncommitted 
        page. This means that you can decommit a range of pages without first 
        determining the current commitment state."  It would be really easy to 
        just do this and re-commit pages each time we allocate, this is 
        apparently expensive, so we might not want to do this. */
    //KORL-PERFORMANCE-000-000-027: memory: uncertain performance characteristics associated with re-committed pages
    const u$ allocatorPages          = korl_math_nextHighestDivision(sizeof(*allocator), pageBytes);
    const u$ allocatorBytes          = allocatorPages * pageBytes;
    const u$ allocationBytes         = allocator->bytes - allocatorBytes;
    void*const allocationRegionStart = KORL_C_CAST(u8*, allocator) + allocatorBytes;
    if(!VirtualFree(allocationRegionStart, allocationBytes, MEM_DECOMMIT))
        korl_logLastError("VirtualFree failed!");
    /* update the allocator metrics */
    allocator->lastAllocation = NULL;
    _korl_heap_linear_allocatorPageGuard(allocator);
}
korl_internal void* korl_heap_linear_allocate(_Korl_Heap_Linear*const allocator, u$ bytes, const wchar_t* file, int line, void* requestedAddress)
{
    void* allocationAddress = NULL;
    _korl_heap_linear_allocatorPageUnguard(allocator);
    const u8*const allocatorEnd = KORL_C_CAST(u8*, allocator) + allocator->bytes;
    /* sanity check that the allocator's address range is page-aligned */
    const u$ pageBytes = korl_memory_pageBytes();
    korl_assert(0 == KORL_C_CAST(u$, allocator) % korl_memory_virtualAlignBytes());
    korl_assert(0 == allocator->bytes           % pageBytes);
    /* If the user has requested a custom address for the allocation, we can 
        choose this address instead of the allocator's next available address.  
        In addition, we can conditionally check to make sure the requested 
        address is valid based on any number of metrics (within the address 
        range of the allocator, not going to overlap another allocation, etc...) */
    const u$ metaBytesRequired = sizeof(_Korl_Heap_Linear_AllocationMeta) 
                               + sizeof(_KORL_HEAP_LINEAR_ALLOCATION_META_SEPARATOR) - 1/*don't include the null terminator*/;
    const u$ allocatorPages = korl_math_nextHighestDivision(sizeof(*allocator), pageBytes);
    const u$ allocatorBytes = allocatorPages * pageBytes;
    _Korl_Heap_Linear_AllocationMeta* metaAddress = KORL_C_CAST(_Korl_Heap_Linear_AllocationMeta*
                                                               ,KORL_C_CAST(u8*, allocator) + allocatorBytes + pageBytes/*skip guard page*/);
    if(allocator->lastAllocation)
    {
        const _Korl_Heap_Linear_AllocationMeta*const lastAllocationMeta = KORL_C_CAST(_Korl_Heap_Linear_AllocationMeta*
                                                                                     ,KORL_C_CAST(u8*, allocator->lastAllocation) - metaBytesRequired);
        korl_assert(lastAllocationMeta->availableBytes % pageBytes == 0);// ensure that the allocation's metadata's available byte count is a multiple of memory page size
        metaAddress = KORL_C_CAST(_Korl_Heap_Linear_AllocationMeta*, 
                                  KORL_C_CAST(u8*, lastAllocationMeta) + lastAllocationMeta->availableBytes + pageBytes/*skip guard page*/);
    }
    const u$ totalAllocationPages = korl_math_nextHighestDivision(metaBytesRequired + bytes, pageBytes);
    const u$ totalAllocationBytes = totalAllocationPages * pageBytes;
    if(requestedAddress)
    {
        /* if the user requested an address, and that address is outside of this 
            heap's virtual memory range, and we have a "next" heap, then we 
            should continue this operation on the "next" heap */
        if(   KORL_C_CAST(u8*, requestedAddress) <  KORL_C_CAST(u8*, allocator) + (allocatorPages * pageBytes)
           || KORL_C_CAST(u8*, requestedAddress) >= KORL_C_CAST(u8*, allocator) + allocator->bytes)
        {
            korl_assert(!"@TODO");
        }
        /* _huge_ simplification here: let's try just requiring the caller to 
            inject allocations at specific addresses in a strictly increasing 
            order; guaranteeing that each allocation is contiguous along the way */
        metaAddress = KORL_C_CAST(_Korl_Heap_Linear_AllocationMeta*, KORL_C_CAST(u8*, requestedAddress) - metaBytesRequired);
        _Korl_Heap_Linear_AllocationMeta*const lastAllocationMeta = allocator->lastAllocation 
                                                                  ? KORL_C_CAST(_Korl_Heap_Linear_AllocationMeta*, KORL_C_CAST(u8*, allocator->lastAllocation) - metaBytesRequired)
                                                                  : NULL;
        /* this address must be _above_ the allocator pages */
        korl_assert(KORL_C_CAST(u8*, allocator) + allocatorBytes + pageBytes/*skip guard page*/ <= KORL_C_CAST(u8*, metaAddress));
        /* this address must be _after_ the last allocation */
        if(lastAllocationMeta)
            korl_assert(KORL_C_CAST(u8*, lastAllocationMeta) + lastAllocationMeta->availableBytes + pageBytes/*guard page*/ <= KORL_C_CAST(u8*, metaAddress));
    }
    /* check to see that this allocation can fit in the range 
        [nextAllocationAddress, lastReservedAddress] */
    if(KORL_C_CAST(u8*, metaAddress) + totalAllocationBytes > allocatorEnd)
    {
        korl_log(WARNING, "linear allocator out of memory");//KORL-ISSUE-000-000-049: memory: logging an error when allocation fails in log module results in poor error logging
        if(!allocator->next)
        {
            /* we want to either (depending on which results in more pages): 
                - double the amount of allocationPages of the next allocator
                - have at _least_ the # of allocationPages required to satisfy `bytes` */
            const u$ newMaxBytes = KORL_MATH_MAX(2*allocator->bytes, allocatorBytes + totalAllocationBytes);
            allocator->next = korl_heap_linear_create(newMaxBytes, NULL);
        }
        allocationAddress = korl_heap_linear_allocate(allocator->next, bytes, file, line, requestedAddress);
        if(!allocationAddress)
            korl_log(ERROR, "system out of memory");
        goto guardAllocator_return_allocationAddress;
    }
    /* commit the pages of this allocation */
    {
        /* MSDN page for VirtualAlloc says:
            "It can commit a page that is already committed. This means you can 
            commit a range of pages, regardless of whether they have already 
            been committed, and the function will not fail." */
        LPVOID resultVirtualAlloc = 
            VirtualAlloc(metaAddress, totalAllocationBytes, MEM_COMMIT, PAGE_READWRITE);
        if(resultVirtualAlloc == NULL)
            korl_logLastError("VirtualAlloc failed!");
        korl_assert(resultVirtualAlloc == metaAddress);
    }
    /* ensure that the space within the allocation's pages that lie outside the 
        range of `bytes` is filled with some known invalid byte pattern */
    if(totalAllocationBytes - (metaBytesRequired + bytes))
        FillMemory(KORL_C_CAST(u8*, metaAddress) + (metaBytesRequired + bytes), totalAllocationBytes - (metaBytesRequired + bytes), _KORL_HEAP_INVALID_BYTE_PATTERN);
    /* ensure that the allocation address is filled with zeros */
    allocationAddress = KORL_C_CAST(u8*, metaAddress) + metaBytesRequired;
    //KORL-PERFORMANCE-000-000-027: memory: uncertain performance characteristics associated with re-committed pages
#if 0/* We don't have to do this, since re-commited pages are guaranteed to be 
        filled with 0.  This is, of course, assuming that our strategy is going 
        to re-commit new pages every time guaranteed. */
    FillMemory(allocationAddress, bytes, 0);
#endif
    /* initialize allocation's metadata */
    metaAddress->allocationMeta.file  = file;
    metaAddress->allocationMeta.line  = line;
    metaAddress->allocationMeta.bytes = bytes;
    metaAddress->previousAllocation   = allocator->lastAllocation;
    metaAddress->availableBytes       = totalAllocationBytes;
    korl_memory_copy(metaAddress + 1, _KORL_HEAP_LINEAR_ALLOCATION_META_SEPARATOR, sizeof(_KORL_HEAP_LINEAR_ALLOCATION_META_SEPARATOR) - 1/*don't include the null terminator*/);
    /* update allocator's metrics */
    allocator->lastAllocation = allocationAddress;
    /**/
    guardAllocator_return_allocationAddress:
        _korl_heap_linear_allocatorPageGuard(allocator);
        return allocationAddress;
}
korl_internal void* korl_heap_linear_reallocate(_Korl_Heap_Linear*const allocator, void* allocation, u$ bytes, const wchar_t* file, int line)
{
    /* sanity checks */
    _korl_heap_linear_allocatorPageUnguard(allocator);
    const u$ pageBytes = korl_memory_pageBytes();
    korl_assert(0 == KORL_C_CAST(u$, allocator) % korl_memory_virtualAlignBytes());
    korl_assert(0 == allocator->bytes           % pageBytes);
    /* ensure that this address is actually within the allocator's range */
    const u$ allocatorPages = korl_math_nextHighestDivision(sizeof(*allocator), pageBytes);
    if(   KORL_C_CAST(u8*, allocation) <  KORL_C_CAST(u8*, allocator) + (allocatorPages * pageBytes)
       || KORL_C_CAST(u8*, allocation) >= KORL_C_CAST(u8*, allocator) + allocator->bytes)
    {
        if(!allocator->next)
            korl_log(ERROR, "allocation not found");
        allocation = korl_heap_linear_reallocate(allocator->next, allocation, bytes, file, line);
        goto guard_allocator_return_allocation;
    }
    korl_assert(   KORL_C_CAST(u8*, allocation) >= KORL_C_CAST(u8*, allocator) + (allocatorPages * pageBytes)
                && KORL_C_CAST(u8*, allocation) <  KORL_C_CAST(u8*, allocator) + allocator->bytes);
    /* ensure that the allocation meta address is divisible by the page size, 
        because this is currently a constraint of this allocator */
    const u$ metaBytesRequired = sizeof(_Korl_Heap_Linear_AllocationMeta) 
                               + sizeof(_KORL_HEAP_LINEAR_ALLOCATION_META_SEPARATOR) - 1/*exclude null terminator*/;
    korl_assert((KORL_C_CAST(u$, allocation) - metaBytesRequired) % pageBytes == 0);
    /* ensure that the memory just before the allocation is the constant meta 
        separator string */
    korl_assert(0 == korl_memory_compare(KORL_C_CAST(u8*, allocation) - (sizeof(_KORL_HEAP_LINEAR_ALLOCATION_META_SEPARATOR) - 1/*exclude null terminator*/)
                                        ,_KORL_HEAP_LINEAR_ALLOCATION_META_SEPARATOR
                                        ,sizeof(_KORL_HEAP_LINEAR_ALLOCATION_META_SEPARATOR) - 1/*exclude null terminator*/));
    /* at this point, it's _fairly_ safe to assume that allocation is a linear allocation */
    _Korl_Heap_Linear_AllocationMeta*const allocationMeta = KORL_C_CAST(_Korl_Heap_Linear_AllocationMeta*, KORL_C_CAST(u8*, allocation) - metaBytesRequired);
    /* if the requested bytes is equal to the current bytes, there is no work to do */
    if(allocationMeta->allocationMeta.bytes == bytes)
        goto guard_allocator_return_allocation;
    /* if the allocation has enough total available space for the caller's 
        requirements, we can just change the size of the allocation in-place & 
        be done with it */
    if(allocationMeta->availableBytes - metaBytesRequired >= bytes)
    {
        /* if we're growing, we need to make sure the new memory is zeroed */
        if(allocationMeta->allocationMeta.bytes < bytes)
            korl_memory_zero(KORL_C_CAST(u8*, allocation) + allocationMeta->allocationMeta.bytes, bytes - allocationMeta->allocationMeta.bytes);
        /* otherwise, we must be shrinking; zero out the memory we're losing 
            just for safety */
        else
        {
            korl_assert(allocationMeta->allocationMeta.bytes > bytes);// sanity check; we should have already handled this case above
            korl_memory_zero(KORL_C_CAST(u8*, allocation) + bytes, allocationMeta->allocationMeta.bytes - bytes);
        }
        allocationMeta->allocationMeta.bytes = bytes;
    }
    /* otherwise, if we are the last allocation we can just commit more pages, 
        assuming we will remain in bounds of the allocator's address pool */
    else if(allocation == allocator->lastAllocation)
    {
        /* if in-place reallocation would take us out-of-bounds of the 
            allocator, we need to either create a new korl-heap or use the next 
            korl-heap in the chain to make the new allocation, copy the data of 
            this allocation to it, then free this allocation */
        const u$ totalAllocationPages = korl_math_nextHighestDivision(metaBytesRequired + bytes, pageBytes);
        const u$ totalAllocationBytes = totalAllocationPages * pageBytes;
        if(KORL_C_CAST(u8*, allocationMeta) + totalAllocationBytes > KORL_C_CAST(u8*, allocator) + allocator->bytes)
        {
            /* create a new allocation in a later part of the korl-heap list */
            if(!allocator->next)
            {
                /* we want to either (depending on which results in more pages): 
                    - double the amount of allocationPages of the next allocator
                    - have at _least_ the # of allocationPages required to satisfy `bytes` */
                const u$ allocatorBytes = allocatorPages * pageBytes;
                const u$ newMaxBytes    = KORL_MATH_MAX(2*allocator->bytes, allocatorBytes + totalAllocationBytes);
                allocator->next = korl_heap_linear_create(newMaxBytes, NULL);
            }
            void*const newAllocation = korl_heap_linear_allocate(allocator->next, bytes, file, line, NULL);
            if(!newAllocation)
                korl_log(ERROR, "system out of memory");
            /* copy data from old allocation to new allocation */
            korl_assert(allocationMeta->allocationMeta.bytes <= bytes);// the old allocation occupied size _must_ be strictly less than the new one (we're growing the allocation)
            korl_memory_copy(newAllocation, allocation, allocationMeta->allocationMeta.bytes);
            /* free the last allocation */
            allocator->lastAllocation = allocationMeta->previousAllocation;
            //KORL-PERFORMANCE-000-000-027: memory: uncertain performance characteristics associated with re-committed pages
            if(!VirtualFree(allocationMeta, allocationMeta->availableBytes, MEM_DECOMMIT))
                korl_logLastError("VirtualFree failed!");
            /**/
            allocation = newAllocation;
        }
        else/* we are growing the last allocation, & we know it will fit in the allocator */
        {
            //KORL-PERFORMANCE-000-000-027: memory: uncertain performance characteristics associated with re-committed pages
            _Korl_Heap_Linear_AllocationMeta*const newMeta = VirtualAlloc(allocationMeta, totalAllocationBytes, MEM_COMMIT, PAGE_READWRITE);
            if(newMeta == NULL)
                korl_logLastError("VirtualAlloc failed!");
            newMeta->availableBytes       = totalAllocationBytes;
            newMeta->allocationMeta.bytes = bytes;
            newMeta->allocationMeta.file  = file;
            newMeta->allocationMeta.line  = line;
            allocation = KORL_C_CAST(u8*, newMeta) + metaBytesRequired;
        }
    }
    /* otherwise, we must allocate->copy->free */
    else
    {
        _korl_heap_linear_allocatorPageGuard(allocator);
        void*const newAllocation = korl_heap_linear_allocate(allocator, bytes, file, line, NULL/*auto-select address*/);
        if(newAllocation)
        {
            korl_memory_copy(newAllocation, allocation, allocationMeta->allocationMeta.bytes);
            korl_heap_linear_free(allocator, allocation, file, line);
        }
        else
            korl_log(ERROR, "failed to create a new reallocation");
        return newAllocation;
    }
    guard_allocator_return_allocation:
        _korl_heap_linear_allocatorPageGuard(allocator);
        return allocation;
}
korl_internal void korl_heap_linear_free(_Korl_Heap_Linear*const allocator, void* allocation, const wchar_t* file, int line)
{
    /* sanity checks */
    _korl_heap_linear_allocatorPageUnguard(allocator);
    const u$ pageBytes = korl_memory_pageBytes();
    korl_assert(0 == KORL_C_CAST(u$, allocator) % korl_memory_virtualAlignBytes());
    korl_assert(0 == allocator->bytes           % pageBytes);
    /* ensure that this address is actually within the allocator's range */
    const u$ allocatorPages = korl_math_nextHighestDivision(sizeof(*allocator), pageBytes);
    if(   KORL_C_CAST(u8*, allocation) <  KORL_C_CAST(u8*, allocator) + (allocatorPages * pageBytes)
       || KORL_C_CAST(u8*, allocation) >= KORL_C_CAST(u8*, allocator) + allocator->bytes)
    {
        if(!allocator->next)
            korl_log(ERROR, "allocation not found");
        korl_heap_linear_free(allocator->next, allocation, file, line);
        goto guard_allocator;
    }
    korl_assert(   KORL_C_CAST(u8*, allocation) >= KORL_C_CAST(u8*, allocator) + (allocatorPages * pageBytes)
                && KORL_C_CAST(u8*, allocation) <  KORL_C_CAST(u8*, allocator) + allocator->bytes);
    /* ensure that the allocation meta address is divisible by the page size, 
        because this is currently a constraint of this allocator */
    const u$ metaBytesRequired = sizeof(_Korl_Heap_Linear_AllocationMeta) 
                               + sizeof(_KORL_HEAP_LINEAR_ALLOCATION_META_SEPARATOR) - 1/*exclude null terminator*/;
    korl_assert((KORL_C_CAST(u$, allocation) - metaBytesRequired) % pageBytes == 0);
    /* ensure that the memory just before the allocation is the constant meta 
        separator string */
    korl_assert(0 == korl_memory_compare(KORL_C_CAST(u8*, allocation) - (sizeof(_KORL_HEAP_LINEAR_ALLOCATION_META_SEPARATOR) - 1/*exclude null terminator*/)
                                        ,_KORL_HEAP_LINEAR_ALLOCATION_META_SEPARATOR
                                        ,sizeof(_KORL_HEAP_LINEAR_ALLOCATION_META_SEPARATOR) - 1/*exclude null terminator*/));
    /* at this point, it's _fairly_ safe to assume that allocation is a linear 
        allocation */
    _Korl_Heap_Linear_AllocationMeta*const allocationMeta = KORL_C_CAST(_Korl_Heap_Linear_AllocationMeta*, KORL_C_CAST(u8*, allocation) - metaBytesRequired);
    /* If we aren't the last allocation, then we're either:
        - in between two allocations
            - set the next allocation's previous pointer to our previous
            - merge our memory region with previous allocation
            - safety memory wipe this allocation
        - the first allocation, with at least one allocation after us
            - set the next allocation's previous pointer to our previous (should be set to NULL)
            - safety memory wipe this allocation */
    if(allocator->lastAllocation != allocation)
    {
        _Korl_Heap_Linear_AllocationMeta*const nextAllocationMeta = 
            KORL_C_CAST(_Korl_Heap_Linear_AllocationMeta*, KORL_C_CAST(u8*, allocationMeta) + allocationMeta->availableBytes + pageBytes/*guard page*/);
        void*const nextAllocation = KORL_C_CAST(u8*, nextAllocationMeta) + metaBytesRequired;
        /* sanity checks for next allocation */
        /* ensure that this address is actually within the allocator's range */
        korl_assert(KORL_C_CAST(u8*, nextAllocation) >= KORL_C_CAST(u8*, allocator) + (allocatorPages * pageBytes)
                 && KORL_C_CAST(u8*, allocation)     <  KORL_C_CAST(u8*, allocator) + allocator->bytes);
        /* ensure that the allocation meta address is divisible by the page size, 
            because this is currently a constraint of this allocator */
        korl_assert((KORL_C_CAST(u$, nextAllocation) - metaBytesRequired) % pageBytes == 0);
        /* ensure that the memory just before the allocation is the constant meta 
            separator string */
        korl_assert(0 == korl_memory_compare(KORL_C_CAST(u8*, nextAllocation) - (sizeof(_KORL_HEAP_LINEAR_ALLOCATION_META_SEPARATOR) - 1/*exclude null terminator*/), 
                                             _KORL_HEAP_LINEAR_ALLOCATION_META_SEPARATOR, 
                                             sizeof(_KORL_HEAP_LINEAR_ALLOCATION_META_SEPARATOR) - 1/*exclude null terminator*/));
        /* everything looks good; we should now be able to update the next allocation's meta data */
        nextAllocationMeta->previousAllocation = allocationMeta->previousAllocation;
        /* increase the previous allocation's available bytes by the appropriate 
            amount, if there is a previous allocation */
        if(allocationMeta->previousAllocation)
        {
            /* sanity checks for previous allocation */
            /* ensure that this address is actually within the allocator's range */
            korl_assert(KORL_C_CAST(u8*, allocationMeta->previousAllocation) >= KORL_C_CAST(u8*, allocator) + (allocatorPages * pageBytes)
                     && KORL_C_CAST(u8*, allocation)                         <  KORL_C_CAST(u8*, allocator) + allocator->bytes);
            /* ensure that the allocation meta address is divisible by the page size, 
                because this is currently a constraint of this allocator */
            korl_assert((KORL_C_CAST(u$, allocationMeta->previousAllocation) - metaBytesRequired) % pageBytes == 0);
            /* ensure that the memory just before the allocation is the constant 
                meta separator string */
            korl_assert(0 == korl_memory_compare(KORL_C_CAST(u8*, allocationMeta->previousAllocation) - (sizeof(_KORL_HEAP_LINEAR_ALLOCATION_META_SEPARATOR) - 1/*exclude null terminator*/), 
                                                 _KORL_HEAP_LINEAR_ALLOCATION_META_SEPARATOR, 
                                                 sizeof(_KORL_HEAP_LINEAR_ALLOCATION_META_SEPARATOR) - 1/*exclude null terminator*/));
            /* we're pretty sure previous allocation is valid */
            _Korl_Heap_Linear_AllocationMeta*const previousAllocationMeta = 
                KORL_C_CAST(_Korl_Heap_Linear_AllocationMeta*, KORL_C_CAST(u8*, allocationMeta->previousAllocation) - metaBytesRequired);
            /* ensure previous allocation is indeed adjacent to us, since that is 
                required by this allocation strategy */
            korl_assert(KORL_C_CAST(u8*, previousAllocationMeta) + previousAllocationMeta->availableBytes + pageBytes/*guard page*/ == KORL_C_CAST(u8*, allocationMeta));
            /* commit the guard page, since the previous allocation will absorb it & 
                potentially use this space in the future */
            if(!VirtualAlloc(KORL_C_CAST(u8*, previousAllocationMeta) + previousAllocationMeta->availableBytes, pageBytes, MEM_COMMIT, PAGE_READWRITE))
                korl_logLastError("VirtualAlloc failed!");
            /* increase the previous allocation's metrics to be able to use this 
                entire region of memory in the future (guardPage + allocation) */
            previousAllocationMeta->availableBytes += pageBytes/*guard page*/ + allocationMeta->availableBytes;
            /* according to the current reallocate algorithm, the memory that this 
                allocation is occupying will be zeroed out if it becomes utilized by 
                previousAllocation */
        }//if there is a previous allocation
        /* to make it easier on ourselves in the debugger, let's at least smash 
            the allocation's meta data */
        allocationMeta->allocationMeta.file = file;
        allocationMeta->allocationMeta.line = line;
        korl_memory_copy(allocationMeta + 1, _KORL_HEAP_LINEAR_ALLOCATION_META_SEPARATOR_FREE, sizeof(_KORL_HEAP_LINEAR_ALLOCATION_META_SEPARATOR_FREE) - 1/*don't include the null terminator*/);
    }// if we're not the last allocation
    /* this is the last allocation; the entire allocation can just be wiped */
    else
    {
        allocator->lastAllocation = allocationMeta->previousAllocation;
        //KORL-PERFORMANCE-000-000-027: memory: uncertain performance characteristics associated with re-committed pages
        if(!VirtualFree(allocationMeta, allocationMeta->availableBytes, MEM_DECOMMIT))
            korl_logLastError("VirtualFree failed!");
    }
    /* if the allocator has no more allocations, we should empty it! */
    if(!allocator->lastAllocation)
    {
        //KORL-PERFORMANCE-000-000-027: memory: uncertain performance characteristics associated with re-committed pages
        const u$ allocatorBytes          = allocatorPages * pageBytes;
        const u$ allocationBytes         = allocator->bytes - allocatorBytes;
        void*const allocationRegionStart = KORL_C_CAST(u8*, allocator) + allocatorBytes;
        if(!VirtualFree(allocationRegionStart, allocationBytes, MEM_DECOMMIT))
            korl_logLastError("VirtualFree failed!");
    }
    guard_allocator:
        _korl_heap_linear_allocatorPageGuard(allocator);
}
korl_internal KORL_HEAP_ENUMERATE(korl_heap_linear_enumerate)
{
    _Korl_Heap_Linear*const allocator = heap;
    _korl_heap_linear_allocatorPageUnguard(allocator);
    const void*const virtualAddressEnd = KORL_C_CAST(u8*, allocator) + allocator->bytes;
    callback(callbackUserData, allocator, virtualAddressEnd);
    if(allocator->next)
        korl_heap_linear_enumerate(callback, callbackUserData, allocator->next);
    _korl_heap_linear_allocatorPageGuard(allocator);
}
korl_internal KORL_MEMORY_ALLOCATOR_ENUMERATE_ALLOCATIONS(korl_heap_linear_enumerateAllocations)
{
    _Korl_Heap_Linear* allocator = KORL_C_CAST(_Korl_Heap_Linear*, allocatorUserData);
    while(allocator)
    {
        /* sanity checks */
        _korl_heap_linear_allocatorPageUnguard(allocator);
        const u$ pageBytes = korl_memory_pageBytes();
        korl_assert(0 == KORL_C_CAST(u$, allocator) % korl_memory_virtualAlignBytes());
        korl_assert(0 == allocator->bytes           % pageBytes);
        /**/
        _korl_heap_linear_enumerateAllocationsRecurse(allocator, allocator->lastAllocation, callback, callbackUserData);
        _Korl_Heap_Linear*const nextAllocator = allocator->next;// we need to do this nonsense because we cannot access `allocator->next` once the allocator page(s) are guarded!
        _korl_heap_linear_allocatorPageGuard(allocator);
        allocator = nextAllocator;
    }
}
