#pragma once
#include "korl-globalDefines.h"
#include "korl-interface-platform.h"
//KORL-ISSUE-000-000-194: algorithm: add code samples for KORL_ALGORITHM_BVH_VOLUME_UNION & KORL_ALGORITHM_BVH_VOLUME_INTERSECTS
#define KORL_ALGORITHM_BVH_VOLUME_UNION(name) void name(void* a, const void* b)
typedef KORL_ALGORITHM_BVH_VOLUME_UNION(fnSig_korl_algorithm_bvh_volumeUnion);
#define KORL_ALGORITHM_BVH_VOLUME_INTERSECTS(name) bool name(const void* a, const void* b)
typedef KORL_ALGORITHM_BVH_VOLUME_INTERSECTS(fnSig_korl_algorithm_bvh_volumeIntersects);
typedef struct Korl_Algorithm_Bvh_CreateInfo
{
    Korl_Memory_AllocatorHandle           allocator;
    u32                                   leafBoundingVolumes;
    u32                                   boundingVolumeStride;
    u32                                   minimumLeafVolumesPerNode;
    const void*                           defaultBoundingVolume;// the user is responsible for making sure *defaultBoundingVolume remains valid for the duration of all calls to BVH APIs
    fnSig_korl_algorithm_bvh_volumeUnion* volumeUnion;
    fnSig_korl_algorithm_compare_context* nodeLeafSortCompare;// the "context" parameter is set to the _Bvh_Node's user-defined volume
} Korl_Algorithm_Bvh_CreateInfo;
/**
 * Usage: call \c korl_algorithm_bvh_create to create a BVH instance, call 
 * \c korl_algorithm_bvh_setBoundingVolume for \c createInfo->leafBoundingVolumes 
 * # of times to initialize the bounding volumes of all the leaf volumes, call 
 * \c korl_algorithm_bvh_build _once_ to build the BVH, then call 
 * \c korl_algorithm_bvh_query any number of times to quickly obtain lists of 
 * leaf volumes which intersect with a given \c boundingVolume
 */
typedef struct Korl_Algorithm_Bvh
{
    Korl_Algorithm_Bvh_CreateInfo createInfo;
    void*                         nodes;// dynamic array of _Bvh_Node; we can't really utilize stb_ds for this unfortunately, because stb_ds needs to know the datatype's size, and we don't actually know what the user defines as a "volume"
    u32                           nodesCapacity;
    u32                           nodesSize;
    // this struct is followed in memory by the following data:
    // (leafBoundingVolumes * boundingVolumeStride) boundingVolumes;
} Korl_Algorithm_Bvh;
korl_internal Korl_Algorithm_Bvh* korl_algorithm_bvh_create(const Korl_Algorithm_Bvh_CreateInfo*const createInfo);
korl_internal void                korl_algorithm_bvh_setBoundingVolume(Korl_Algorithm_Bvh* context, u32 leafBoundingVolumeIndex, const void* boundingVolume);
korl_internal void                korl_algorithm_bvh_build(Korl_Algorithm_Bvh* context);
/** \param boundingVolume a pointer to a user-defined struct whose type matches 
 * that of \c context->createInfo.defaultBoundingVolume
 * \return an array of user-defined BoundingVolume structs, whose size is 
 * determined by \c context->createInfo.boundingVolumeStride , allocated using 
 * the same allocator as \c context->createInfo.allocator , and whose elements 
 * should all be intersecting \c *boundingVolume */
korl_internal void*               korl_algorithm_bvh_query(Korl_Algorithm_Bvh* context, const void* boundingVolume, u$* o_resultArraySize, fnSig_korl_algorithm_bvh_volumeIntersects* volumeIntersects);
typedef struct Korl_Algorithm_GraphDirected_CreateInfo
{
    Korl_Memory_AllocatorHandle allocator;
    u32                         nodesSize;
} Korl_Algorithm_GraphDirected_CreateInfo;
typedef struct Korl_Algorithm_GraphDirected
{
    Korl_Algorithm_GraphDirected_CreateInfo        createInfo;
    struct _Korl_Algorithm_GraphDirected_NodeMeta* nodeMetas;
    struct _Korl_Algorithm_GraphDirected_Edge*     edges;
    u32                                            edgesCapacity;
    u32                                            edgesSize;
} Korl_Algorithm_GraphDirected;
korl_internal Korl_Algorithm_GraphDirected korl_algorithm_graphDirected_create(const Korl_Algorithm_GraphDirected_CreateInfo*const createInfo);
korl_internal void                         korl_algorithm_graphDirected_destroy(Korl_Algorithm_GraphDirected* context);
korl_internal void                         korl_algorithm_graphDirected_addEdge(Korl_Algorithm_GraphDirected* context, u32 indexParent, u32 indexChild);
/** \param o_resultIndexArray _must_ be >= \c context->createInfo.nodesSize in length
 * \return \c true if topological sort succeeds, \c false if a cycle is detected */
korl_internal bool                         korl_algorithm_graphDirected_sortTopological(Korl_Algorithm_GraphDirected* context, u32* o_resultIndexArray);
/** \return An array of indices into the user's nodes array sorted topologically, 
 * such that parent node indices _always_ come _before_ their child node indices.  
 * If no such order exists (the graph created by the user contains cycles), then 
 * \c NULL is returned.  The result array is allocated via the \c allocator 
 * parameter. */
korl_internal u32*                         korl_algorithm_graphDirected_sortTopologicalNew(Korl_Algorithm_GraphDirected* context, Korl_Memory_AllocatorHandle allocator);
