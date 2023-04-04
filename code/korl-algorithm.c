#include "korl-algorithm.h"
#include <stdlib.h>
#if defined(KORL_PLATFORM)
    korl_internal KORL_FUNCTION_korl_algorithm_sort_quick(korl_algorithm_sort_quick)
    {
        qsort(array, arraySize, arrayStride, compare);
    }
    #ifdef KORL_PLATFORM_WINDOWS
        /* in case the reader is wondering, the only reason why any of this 
            Windows-specific shit exists is simply because some dickhead at 
            microshaft thought it was a great idea to change the function 
            signature of the compare callback to qsort_s such that the context 
            is the first parameter instead of the last */
        typedef struct _Korl_Windows_Algorithm_Sort_Quick_Context
        {
            void* context;
            fnSig_korl_algorithm_compare_context* compare;
        } _Korl_Windows_Algorithm_Sort_Quick_Context;
        int _korl_windows_algorithm_sort_quick_compareWrapper(void* context, const void* a, const void* b)
        {
            _Korl_Windows_Algorithm_Sort_Quick_Context* contextWrapper = context;
            return contextWrapper->compare(a, b, contextWrapper->context);
        }
    #endif
    korl_internal KORL_FUNCTION_korl_algorithm_sort_quick_context(korl_algorithm_sort_quick_context)
    {
        #ifdef KORL_PLATFORM_WINDOWS
            _Korl_Windows_Algorithm_Sort_Quick_Context windows_crt_is_a_piece_of_shit_wrapper = {context, compare};
            qsort_s(array, arraySize, arrayStride, _korl_windows_algorithm_sort_quick_compareWrapper, &windows_crt_is_a_piece_of_shit_wrapper);
        #else
            qsort_s(array, arraySize, arrayStride, compare, context);
        #endif
    }
#endif
typedef struct _Korl_Algorithm_Bvh_Node
{
    u32 leafBoundingVolumeIndexStart;
    u32 leafBoundingVolumeIndexEnd;
    u32 bvhNodeChildIndexOffsetLeft;
    u32 bvhNodeChildIndexOffsetRight;
    // this struct should be followed by user-defined volume struct, which is where we will store this node's total volume
    //  @TODO: this is currently a waste of space, as the user-defined volume also includes a pointer back to user-defined struct that is encapsulated by the volume
    //         consider making the user-defined volume struct _just_ be the AABB, and just 
} _Korl_Algorithm_Bvh_Node;
korl_internal Korl_Algorithm_Bvh* korl_algorithm_bvh_create(const Korl_Algorithm_Bvh_CreateInfo*const createInfo)
{
    korl_assert(createInfo->minimumLeafVolumesPerNode > 0);
    Korl_Algorithm_Bvh*const result = KORL_C_CAST(Korl_Algorithm_Bvh*, korl_allocate(createInfo->allocator, sizeof(Korl_Algorithm_Bvh) + (createInfo->leafBoundingVolumes * createInfo->boundingVolumeStride)));
    result->createInfo    = *createInfo;
    result->nodesCapacity = 8;
    result->nodes         = korl_allocate(createInfo->allocator, result->nodesCapacity * (sizeof(_Korl_Algorithm_Bvh_Node) + createInfo->boundingVolumeStride));
    /* the first node is always the root, which we can initialize with the default bounding volume */
    result->nodesSize = 1;
    _Korl_Algorithm_Bvh_Node*const rootNode = KORL_C_CAST(_Korl_Algorithm_Bvh_Node*, result->nodes/*no need to do fancy byte offsets, since this is the first node*/);
    rootNode->leafBoundingVolumeIndexEnd = createInfo->leafBoundingVolumes;
    void*const rootBoundingVolume = rootNode + 1;
    korl_memory_copy(rootBoundingVolume, createInfo->defaultBoundingVolume, createInfo->boundingVolumeStride);
    return result;
}
korl_internal void korl_algorithm_bvh_setBoundingVolume(Korl_Algorithm_Bvh* context, u32 leafBoundingVolumeIndex, const void* boundingVolume)
{
    korl_assert(leafBoundingVolumeIndex < context->createInfo.leafBoundingVolumes);
    korl_memory_copy(KORL_C_CAST(u8*, context + 1) + leafBoundingVolumeIndex * context->createInfo.boundingVolumeStride, boundingVolume, context->createInfo.boundingVolumeStride);
}
korl_internal void _korl_algorithm_bvh_buildRecursive(Korl_Algorithm_Bvh*const context, u32 nodeIndex)
{
    const u$ nodeStride = sizeof(_Korl_Algorithm_Bvh_Node) + context->createInfo.boundingVolumeStride;
    _Korl_Algorithm_Bvh_Node* node = KORL_C_CAST(_Korl_Algorithm_Bvh_Node*, KORL_C_CAST(u8*, context->nodes) + nodeIndex * nodeStride);
    /* all nodes must first accumulate their bounding volume */
    korl_assert(node->leafBoundingVolumeIndexStart < node->leafBoundingVolumeIndexEnd);
    void*const nodeVolume          = node    + 1;
    void*const leafVolumes         = context + 1;
    void*const nodeLeafVolumes     = KORL_C_CAST(u8*, leafVolumes) + (node->leafBoundingVolumeIndexStart * context->createInfo.boundingVolumeStride);
    const u32  nodeLeafVolumesSize = node->leafBoundingVolumeIndexEnd - node->leafBoundingVolumeIndexStart;
    for(u32 i = 0; i < nodeLeafVolumesSize; i++)
    {
        const void*const leafVolume = KORL_C_CAST(u8*, nodeLeafVolumes) + (i * context->createInfo.boundingVolumeStride);
        context->createInfo.volumeUnion(nodeVolume, leafVolume);
    }
    /* if this node contains <= the minimum leaf node threshold, we're done */
    if(nodeLeafVolumesSize <= context->createInfo.minimumLeafVolumesPerNode)
        return;
    /* this node must be split; we need to: 
        - sort this node's range of leafVolumes using a user-defined comparator, and the nodeVolume as the context
        - allocate 2 new nodes
        - assign the 2 new nodes their respective leafVolume ranges
            - we can do this by iterating over the leafVolumes & invoking nodeLeafSortCompare; 
              if the compare result is non-zero, that defines the boundary
            - if we somehow don't find a boundary (due to numerical error or some nonsense), just split down the middle */
    korl_algorithm_sort_quick_context(nodeLeafVolumes, nodeLeafVolumesSize, context->createInfo.boundingVolumeStride, context->createInfo.nodeLeafSortCompare, nodeVolume);
    u32 nodeLeafVolumesSplitIndex = 1;// @TODO: performance: we could technically calculate this in the first loop over nodeLeafVolumesSize (when we're calculating the node's volume union), but this would require a separate function callback since the nodes are not yet ordered
    for(; nodeLeafVolumesSplitIndex < nodeLeafVolumesSize; nodeLeafVolumesSplitIndex++)
    {
        const void*const leaf         = KORL_C_CAST(u8*, nodeLeafVolumes) + ( nodeLeafVolumesSplitIndex      * context->createInfo.boundingVolumeStride);
        const void*const leafPrevious = KORL_C_CAST(u8*, nodeLeafVolumes) + ((nodeLeafVolumesSplitIndex - 1) * context->createInfo.boundingVolumeStride);
        if(0 != context->createInfo.nodeLeafSortCompare(leafPrevious, leaf, nodeVolume))
            break;
    }
    if(nodeLeafVolumesSplitIndex >= nodeLeafVolumesSize)
        nodeLeafVolumesSplitIndex = nodeLeafVolumesSize / 2;
    /* allocate 2 new nodes & assign their respective leafVolume ranges based on nodeLeafVolumesSplitIndex */
    if(context->nodesSize + 2 > context->nodesCapacity)
    {
        context->nodesCapacity *= 2;
        context->nodes          = korl_reallocate(context->createInfo.allocator, context->nodes, context->nodesCapacity * (sizeof(_Korl_Algorithm_Bvh_Node) + context->createInfo.boundingVolumeStride));
        node                    = KORL_C_CAST(_Korl_Algorithm_Bvh_Node*, KORL_C_CAST(u8*, context->nodes) + nodeIndex * nodeStride);// re-acquire, since context->nodes potentially changed
    }
    const u32 nodeIdA = node->bvhNodeChildIndexOffsetLeft  = context->nodesSize++;
    const u32 nodeIdB = node->bvhNodeChildIndexOffsetRight = context->nodesSize++;
    _Korl_Algorithm_Bvh_Node*const nodeChildA = KORL_C_CAST(_Korl_Algorithm_Bvh_Node*, KORL_C_CAST(u8*, context->nodes) + node->bvhNodeChildIndexOffsetLeft  * nodeStride);
    _Korl_Algorithm_Bvh_Node*const nodeChildB = KORL_C_CAST(_Korl_Algorithm_Bvh_Node*, KORL_C_CAST(u8*, context->nodes) + node->bvhNodeChildIndexOffsetRight * nodeStride);
    korl_memory_copy(nodeChildA + 1, context->createInfo.defaultBoundingVolume, context->createInfo.boundingVolumeStride);
    korl_memory_copy(nodeChildB + 1, context->createInfo.defaultBoundingVolume, context->createInfo.boundingVolumeStride);
    nodeChildA->leafBoundingVolumeIndexStart = node->leafBoundingVolumeIndexStart;
    nodeChildA->leafBoundingVolumeIndexEnd   = node->leafBoundingVolumeIndexStart + nodeLeafVolumesSplitIndex;
    nodeChildB->leafBoundingVolumeIndexStart = node->leafBoundingVolumeIndexStart + nodeLeafVolumesSplitIndex;
    nodeChildB->leafBoundingVolumeIndexEnd   = node->leafBoundingVolumeIndexEnd;
    /* we use local nodeId vars here because context->nodes can be reallocated 
        at any time during these recursive calls, which would invalidate our 
        _Korl_Algorithm_Bvh_Node pointers, but the index of context->nodes for each node is 
        immutable */
    _korl_algorithm_bvh_buildRecursive(context, nodeIdA);
    _korl_algorithm_bvh_buildRecursive(context, nodeIdB);
}
korl_internal void korl_algorithm_bvh_build(Korl_Algorithm_Bvh* context)
{
    if(context->createInfo.leafBoundingVolumes <= 0)
        return;// nothing to build if there are no leaf volumes!
    _korl_algorithm_bvh_buildRecursive(context, 0/*index 0 => the root node*/);
}
korl_internal void _korl_algorithm_bvh_queryRecursive(Korl_Algorithm_Bvh* context, u32 nodeIndex, const void* boundingVolume, void** pResult, u$* pResultCapacity, u$* o_resultArraySize, fnSig_korl_algorithm_bvh_volumeIntersects* volumeIntersects)
{
    const u$ nodeStride = sizeof(_Korl_Algorithm_Bvh_Node) + context->createInfo.boundingVolumeStride;
    _Korl_Algorithm_Bvh_Node* node = KORL_C_CAST(_Korl_Algorithm_Bvh_Node*, KORL_C_CAST(u8*, context->nodes) + nodeIndex * nodeStride);
    void*const nodeVolume = node + 1;
    if(!volumeIntersects(nodeVolume, boundingVolume))
        /* if the boundingVolume doesn't intersect this node's volume, then it 
            is impossible for us to be intersecting with any of this node's leaf 
            volumes; we're done */
        return;
    if(node->bvhNodeChildIndexOffsetLeft || node->bvhNodeChildIndexOffsetRight)
    {
        /* this node is divided; we need to recursively query each child */
        korl_assert(node->bvhNodeChildIndexOffsetLeft && node->bvhNodeChildIndexOffsetRight);
        _korl_algorithm_bvh_queryRecursive(context, node->bvhNodeChildIndexOffsetLeft , boundingVolume, pResult, pResultCapacity, o_resultArraySize, volumeIntersects);
        _korl_algorithm_bvh_queryRecursive(context, node->bvhNodeChildIndexOffsetRight, boundingVolume, pResult, pResultCapacity, o_resultArraySize, volumeIntersects);
    }
    else
    {
        /* this node has no children, and boundingVolume is intersecting it; we 
            must invoke volumeIntersects on all leafVolumes associated with this node */
        void*const leafVolumes = context + 1;
        for(u32 i = node->leafBoundingVolumeIndexStart; i < node->leafBoundingVolumeIndexEnd; i++)
        {
            const void*const leafVolume = KORL_C_CAST(u8*, leafVolumes) + (i * context->createInfo.boundingVolumeStride);
            if(!volumeIntersects(leafVolume, boundingVolume))
                continue;
            if(*o_resultArraySize + 1 > *pResultCapacity)
            {
                *pResultCapacity *= 2;
                *pResult = korl_reallocate(context->createInfo.allocator, *pResult, *pResultCapacity * context->createInfo.boundingVolumeStride);
            }
            const u$    resultVolumeIndex = (*o_resultArraySize)++;
            void* const resultVolume      = KORL_C_CAST(u8*, *pResult) + resultVolumeIndex * context->createInfo.boundingVolumeStride;
            korl_memory_copy(resultVolume, leafVolume, context->createInfo.boundingVolumeStride);
        }
    }
}
korl_internal void* korl_algorithm_bvh_query(Korl_Algorithm_Bvh* context, const void* boundingVolume, u$* o_resultArraySize, fnSig_korl_algorithm_bvh_volumeIntersects* volumeIntersects)
{
    u$    resultCapacity = 8;
    void* result         = korl_allocate(context->createInfo.allocator, resultCapacity * context->createInfo.boundingVolumeStride);
    *o_resultArraySize = 0;
    _korl_algorithm_bvh_queryRecursive(context, 0/*index 0 => the root node*/, boundingVolume, &result, &resultCapacity, o_resultArraySize, volumeIntersects);
    if(*o_resultArraySize <= 0)
    {
        korl_free(context->createInfo.allocator, result);
        result = NULL;
    }
    return result;
}
