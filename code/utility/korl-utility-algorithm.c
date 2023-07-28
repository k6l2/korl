#include "utility/korl-utility-algorithm.h"
typedef struct _Korl_Algorithm_Bvh_Node
{
    u32 leafBoundingVolumeIndexStart;
    u32 leafBoundingVolumeIndexEnd;
    u32 bvhNodeChildIndexOffsetLeft;
    u32 bvhNodeChildIndexOffsetRight;
    // this struct should be followed by user-defined volume struct, which is where we will store this node's total volume
    //KORL-ISSUE-000-000-154: algorithm/bvh: this is currently a waste of space, as the user-defined volume also includes a pointer back to user-defined struct that is encapsulated by the volume; consider making the user-defined volume struct _just_ be the AABB
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
    u32 nodeLeafVolumesSplitIndex = 1;//KORL-PERFORMANCE-000-000-049: algorithm/bvh: we could technically calculate this in the first loop over nodeLeafVolumesSize (when we're calculating the node's volume union), but this would require a separate function callback since the nodes are not yet ordered
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
typedef struct _Korl_Algorithm_GraphDirected_Edge
{
    u32 indexParent;
    u32 indexChild;
} _Korl_Algorithm_GraphDirected_Edge;
typedef struct _Korl_Algorithm_GraphDirected_NodeMeta
{
    u32 children;
    u32 childEdgesIndex;// index into the graphs `edges` member; only valid if `children` != 0
    u32 inDegree;
} _Korl_Algorithm_GraphDirected_NodeMeta;
korl_internal Korl_Algorithm_GraphDirected korl_algorithm_graphDirected_create(const Korl_Algorithm_GraphDirected_CreateInfo*const createInfo)
{
    KORL_ZERO_STACK(Korl_Algorithm_GraphDirected, result);
    result.createInfo    = *createInfo;
    result.nodeMetas     = KORL_C_CAST(_Korl_Algorithm_GraphDirected_NodeMeta*, korl_allocate(createInfo->allocator, createInfo->nodesSize * sizeof(*result.nodeMetas)));
    result.edgesCapacity = createInfo->nodesSize;
    result.edges         = KORL_C_CAST(_Korl_Algorithm_GraphDirected_Edge*, korl_allocate(createInfo->allocator, result.edgesCapacity * sizeof(*result.edges)));
    return result;
}
korl_internal void korl_algorithm_graphDirected_addEdge(Korl_Algorithm_GraphDirected* context, u32 indexParent, u32 indexChild)
{
    korl_assert(context->edgesSize <= context->edgesCapacity);
    if(context->edgesSize == context->edgesCapacity)
    {
        context->edgesCapacity = KORL_MATH_MAX(2 * context->edgesCapacity, context->edgesCapacity + 1);
        context->edges         = KORL_C_CAST(_Korl_Algorithm_GraphDirected_Edge*, korl_reallocate(context->createInfo.allocator, context->edges, context->edgesCapacity * sizeof(*context->edges)));
    }
    context->edges[context->edgesSize++] = KORL_STRUCT_INITIALIZE(_Korl_Algorithm_GraphDirected_Edge){.indexParent = indexParent, .indexChild = indexChild};
}
korl_internal KORL_ALGORITHM_COMPARE(_korl_algorithm_graphDirected_sortTopological_edgeSortCompare_ascendIndexParent_ascendIndexChild)
{
    _Korl_Algorithm_GraphDirected_Edge*const edgeA = KORL_C_CAST(_Korl_Algorithm_GraphDirected_Edge*, a);
    _Korl_Algorithm_GraphDirected_Edge*const edgeB = KORL_C_CAST(_Korl_Algorithm_GraphDirected_Edge*, b);
    return edgeA->indexParent == edgeB->indexParent 
           ? edgeA->indexChild  > edgeB->indexChild 
           : edgeA->indexParent > edgeB->indexParent;
}
korl_internal u32* korl_algorithm_graphDirected_sortTopological(Korl_Algorithm_GraphDirected* context, Korl_Memory_AllocatorHandle allocator)
{
    // loosely derived from: https://www.goeduhub.com/9919/topological-sort-in-c
    /* sort the list of edges by `indexParent` (order doesn't really matter here I don't think...);
        this allows us to store & use a list of child nodes for all nodes - O(e * log(e))*/
    korl_algorithm_sort_quick(context->edges, context->edgesSize, sizeof(*context->edges)
                             ,_korl_algorithm_graphDirected_sortTopological_edgeSortCompare_ascendIndexParent_ascendIndexChild);
    /* iterate over all edges & accumulate the "in-degree" of all nodes, where 
        "in-degree" of a graph vertex is just the # of edges which point to that 
        vertex; in the case of GraphDirected, we can simply count the # of edges 
        whose indexChild == the node's index; in addition, update 
        `context->nodeMetas` with child information - O(e) */
    for(u32 e = 0; e < context->edgesSize; e++)
    {
        // if this is the first edge where this node is the parent, we assign this edge index as the start of this node's "child array"
        if(0 == context->nodeMetas[context->edges[e].indexParent].children)
            context->nodeMetas[context->edges[e].indexParent].childEdgesIndex = e;
        //@TODO: if this isn't the first edge for this parent node, ensure that the previous edge is different, since edges should also be sorted in ascending indexChild values
        context->nodeMetas[context->edges[e].indexParent].children++;
        context->nodeMetas[context->edges[e].indexChild].inDegree++;
    }
    /* allocate a node index deque; queue all nodes with in-degree == 0; the 
        `resultDeque` will by used by enqueueing items to the back of the array 
        (where "back" is determined by "front" + size), and dequeueing items 
        from the front (advancing the front offset simultaneously) - O(n) */
    u32* resultDeque      = KORL_C_CAST(u32*, korl_allocateDirty(allocator, context->createInfo.nodesSize * sizeof(*resultDeque)));
    u32  resultDequeSize  = 0;
    u32  resultDequeFront = 0;
    for(u32 n = 0; n < context->createInfo.nodesSize; n++)
        if(0 == context->nodeMetas[n].inDegree)
            resultDeque[resultDequeFront + resultDequeSize++] = n;
    /* perform topological sort using deque recursion to visit all nodes */
    while(resultDequeSize)
    {
        /* dequeue the next node */
        const u32 nextNode = resultDeque[resultDequeFront++];
        resultDequeSize--;
        /* reduce in-degree of all adjacent nodes by 1, and queue all those whose in-degree is now 0 */
        _Korl_Algorithm_GraphDirected_Edge*const nodeEdges = context->edges + context->nodeMetas[nextNode].childEdgesIndex;
        for(u32 c = 0; c < context->nodeMetas[nextNode].children; c++)
        {
            korl_assert(nodeEdges[c].indexParent == nextNode);
            if(0 == --context->nodeMetas[nodeEdges[c].indexChild].inDegree)
                resultDeque[resultDequeFront + resultDequeSize++] = nodeEdges[c].indexChild;
        }
    }
    /* if a cycle was detected, dispose of the deque and give the user nothing */
    if(resultDequeFront < context->createInfo.nodesSize)
    {
        korl_free(allocator, resultDeque);
        return NULL;
    }
    korl_assert(resultDequeFront == context->createInfo.nodesSize);
    return resultDeque;
}
