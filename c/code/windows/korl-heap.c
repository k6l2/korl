#include "korl-heap.h"
#include "korl-memory.h"
#include "korl-windows-globalDefines.h"
#include "korl-interface-platform.h"// for KorlEnumLogLevel
#define _KORL_HEAP_GENERAL_GUARD_UNUSED_ALLOCATION_PAGES 1
#define _KORL_HEAP_GENERAL_GUARD_ALLOCATOR 1
#define _KORL_HEAP_GENERAL_LAZY_COMMIT_PAGES 1
#define _KORL_HEAP_GENERAL_DEBUG 0
korl_global_const i8 _KORL_HEAP_GENERAL_ALLOCATION_META_SEPARATOR[]     = "[KORL-general-allocation]";
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
    if(!VirtualFree(allocator, 0/*release everything*/, MEM_RELEASE))
        korl_logLastError("VirtualFree failed!");
}
korl_internal void korl_heap_general_empty(_Korl_Heap_General* allocator)
{
    /* sanity checks */
    _korl_heap_general_allocatorPagesUnguard(allocator);
    const u$ pageBytes = korl_memory_pageBytes();
    korl_assert(0 == KORL_C_CAST(u$, allocator) % korl_memory_virtualAlignBytes());
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
       || availablePageIndex + allocationPages >= allocator->allocationPages/*required only because of KORL-ISSUE-000-000-088*/)
    {
        korl_log(WARNING, "occupyAvailablePages failed!  (allocator may require defragmentation, or may have run out of space)");
        if(availablePageIndex + allocationPages >= allocator->allocationPages)
            _korl_heap_general_setPageFlags(allocator, availablePageIndex, allocationPages, false);//required only because of KORL-ISSUE-000-000-088
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
    korl_assert(allocation >= allocationRegionBegin);// we must be inside the allocation region
    korl_assert(allocation <  allocationRegionEnd);  // we must be inside the allocation region
    /* access the allocation meta data */
    const u$ metaBytesRequired = sizeof(_Korl_Heap_General_AllocationMeta) + sizeof(_KORL_HEAP_GENERAL_ALLOCATION_META_SEPARATOR) - 1/*don't include null-terminator*/;
    _Korl_Heap_General_AllocationMeta*const allocationMeta = KORL_C_CAST(_Korl_Heap_General_AllocationMeta*, 
        KORL_C_CAST(u8*, allocation) - metaBytesRequired);
#if _KORL_HEAP_GENERAL_DEBUG
    const u$ allocationPages = allocationMeta->pagesCommitted;
#endif
    /* allocation sanity checks */
    korl_assert(0 == KORL_C_CAST(u$, allocationMeta) % pageBytes);// we must be page-aligned
    korl_assert(0 == korl_memory_compare(allocationMeta + 1, _KORL_HEAP_GENERAL_ALLOCATION_META_SEPARATOR, sizeof(_KORL_HEAP_GENERAL_ALLOCATION_META_SEPARATOR) - 1/*don't include null-terminator*/));
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
    korl_assert(newAllocationPages > allocationMeta->pagesCommitted);
#if 1
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
#endif
    /* we were unable to quickly expand the allocation; we need to allocate=>copy=>free */
    /* clear our page flags */
    _korl_heap_general_setPageFlags(allocator, allocationPage, allocationMeta->pagesCommitted, false/*unoccupied*/);
    /* occupy new page flags that satisfy newAllocationPages */
    const u$ newAllocationPage = _korl_heap_general_occupyAvailablePages(allocator, newAllocationPages);
    if(newAllocationPage >= allocator->allocationPages)
    {
        korl_log(WARNING, "occupyAvailablePages failed!  (allocator may require defragmentation, or may have run out of space)");
        _korl_heap_general_setPageFlags(allocator, allocationPage, allocationMeta->pagesCommitted, true/*occupied*/);
        allocation = NULL;
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
    _korl_heap_general_allocatorPagesGuard(allocator);
#if _KORL_HEAP_GENERAL_DEBUG
    u$ allocationCountEnd, occupiedPageCountEnd;
    _korl_heap_general_checkIntegrity(allocator, &allocationCountEnd, &occupiedPageCountEnd);
    korl_assert(allocationCountEnd   == allocationCountBegin - 1);
    korl_assert(occupiedPageCountEnd == occupiedPageCountBegin - allocationPages);
#endif
}
korl_internal KORL_MEMORY_ALLOCATOR_ENUMERATE_ALLOCATIONS(korl_heap_general_enumerateAllocations)
{
    _Korl_Heap_General*const allocator = KORL_C_CAST(_Korl_Heap_General*, allocatorUserData);
    /* sanity checks */
    _korl_heap_general_allocatorPagesUnguard(allocator);
    korl_assert(0 == KORL_C_CAST(u$, allocator) % korl_memory_virtualAlignBytes());
    /* calculate virtual address range */
    const u$ pageBytes      = korl_memory_pageBytes();
    const u$ allocatorPages = korl_math_nextHighestDivision(sizeof(*allocator) + allocator->availablePageFlagsSize*sizeof(*(allocator->availablePageFlags)), pageBytes);
    if(out_allocatorVirtualAddressEnd)
        *out_allocatorVirtualAddressEnd = KORL_C_CAST(u8*, allocator) + allocatorPages*pageBytes + allocator->allocationPages*pageBytes;
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
                goto guardAllocator_return;
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
    guardAllocator_return:
    _korl_heap_general_allocatorPagesGuard(allocator);
}
