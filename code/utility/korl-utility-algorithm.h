#pragma once
#include "korl-globalDefines.h"
#include "korl-interface-platform.h"
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
    void*           nodes;// dynamic array of _Bvh_Node; we can't really utilize stb_ds for this unfortunately, because stb_ds needs to know the datatype's size, and we don't actually know what the user defines as a "volume"
    u32             nodesCapacity;
    u32             nodesSize;
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
