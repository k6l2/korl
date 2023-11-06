#pragma once
#include "korl-globalDefines.h"
#include "korl-interface-platform.h"
/** # Korl_Algorithm_Bvh Example Usage Code
 * 
 * ## Data & Callbacks
 * 
 * ```
 * typedef struct EntityManager_BoundingVolume
 * {
 *     Entity*            entity;
 *     Korl_Math_Aabb2f32 aabb;
 *     Korl_Math_V2f32    averageMid;
 * } EntityManager_BoundingVolume;
 * 
 * korl_internal KORL_ALGORITHM_BVH_VOLUME_UNION(_entityManager_bvh_volumeUnion)
 * {
 *     EntityManager_BoundingVolume*const       vA     = KORL_C_CAST(EntityManager_BoundingVolume*      , a);
 *     const EntityManager_BoundingVolume*const vB     = KORL_C_CAST(const EntityManager_BoundingVolume*, b);
 *     const Korl_Math_V2f32                    vASize = korl_math_aabb2f32_size(vA->aabb);
 *     if(vASize.x <= 0)
 *         vA->averageMid.x = vB->averageMid.x;
 *     else
 *         vA->averageMid.x = 0.5f * (vA->averageMid.x + vB->averageMid.x);
 *     if(vASize.y <= 0)
 *         vA->averageMid.y = vB->averageMid.y;
 *     else
 *         vA->averageMid.y = 0.5f * (vA->averageMid.y + vB->averageMid.y);
 *     vA->aabb = korl_math_aabb2f32_union(vA->aabb, vB->aabb);
 * }
 * 
 * korl_internal KORL_ALGORITHM_BVH_VOLUME_INTERSECTS(_entityManager_bvh_volumeIntersects)
 * {
 *     const EntityManager_BoundingVolume*const vA = KORL_C_CAST(const EntityManager_BoundingVolume*, a);
 *     const EntityManager_BoundingVolume*const vB = KORL_C_CAST(const EntityManager_BoundingVolume*, b);
 *     return korl_math_aabb2f32_areIntersecting(vA->aabb, vB->aabb);
 * }
 * 
 * korl_internal KORL_ALGORITHM_COMPARE_CONTEXT(_entityManager_bvh_nodeLeafSortCompare)
 * {
 *     const EntityManager_BoundingVolume*const leafA    = KORL_C_CAST(const EntityManager_BoundingVolume*, a);
 *     const EntityManager_BoundingVolume*const leafB    = KORL_C_CAST(const EntityManager_BoundingVolume*, b);
 *     const EntityManager_BoundingVolume*const node     = KORL_C_CAST(const EntityManager_BoundingVolume*, context);
 *     const Korl_Math_V2f32                    nodeSize = korl_math_aabb2f32_size(node->aabb);
 *     f32 nodeToLeafA;
 *     f32 nodeToLeafB;
 *     if(nodeSize.x > nodeSize.y)// split along the x-axis
 *     {
 *         nodeToLeafA = leafA->averageMid.x - node->averageMid.x;
 *         nodeToLeafB = leafB->averageMid.x - node->averageMid.x;
 *     }
 *     else// split along the y-axis
 *     {
 *         nodeToLeafA = leafA->averageMid.y - node->averageMid.y;
 *         nodeToLeafB = leafB->averageMid.y - node->averageMid.y;
 *     }
 *     return nodeToLeafA * nodeToLeafB < 0 
 *            // the leaves are on opposite sides of the node's averageMid
 *            ? nodeToLeafA < 0 
 *              ? -1 // a comes before b
 *              :  1 // b comes before a
 *            // both nodes are on the same side
 *            : 0;
 * }
 * ```
 * 
 * ## BVH Construction
 * 
 * ```
 * korl_shared_const EntityManager_BoundingVolume DEFAULT_BOUNDING_VOLUME = {NULL, KORL_MATH_AABB2F32_EMPTY};
 * const Korl_Algorithm_Bvh_CreateInfo createInfoBvh{.allocator             = allocatorFrame
 *                                                  ,.leafBoundingVolumes   = activeEntities
 *                                                  ,.boundingVolumeStride  = sizeof(EntityManager_BoundingVolume)
 *                                                  ,.maxLeafVolumesPerNode = 3
 *                                                  ,.defaultBoundingVolume = &DEFAULT_BOUNDING_VOLUME
 *                                                  ,.volumeUnion           = _entityManager_bvh_volumeUnion
 *                                                  ,.nodeLeafSortCompare   = _entityManager_bvh_nodeLeafSortCompare};
 * bvh = korl_algorithm_bvh_create(&createInfoBvh);
 * {
 *     u32 activeEntityIndex = 0;
 *     for(Entity* entity = entities; entity < entitiesEnd; entity++)
 *     {
 *         if(!entity->active)
 *             continue;
 *         Korl_Math_Aabb2f32 entityAabb = entityAabbs[entity - entities];
 *         EntityManager_BoundingVolume boundingVolume = {.entity     = entity
 *                                                       ,.aabb       = entityAabb
 *                                                       ,.averageMid = entityAabb.min + 0.5f*korl_math_aabb2f32_size(entityAabb)};
 *         korl_algorithm_bvh_setBoundingVolume(bvh, activeEntityIndex++, &boundingVolume);
 *     }
 *     korl_assert(activeEntityIndex == activeEntities);// ensure that all active entities have been added to the BVH
 * }
 * korl_algorithm_bvh_build(bvh);
 * ```
 * 
 * ## BVH Query & Enumeration
 * 
 * ```
 * u$ bvhManifoldSize = 0;
 * EntityManager_BoundingVolume*const bvhManifold = KORL_C_CAST(EntityManager_BoundingVolume*
 *                                                             ,korl_algorithm_bvh_query(bvh, &boundingVolume, &bvhManifoldSize, _entityManager_bvh_volumeIntersects));
 * // cull BoundingVolumes that are _not_ unique; BVH does _not_ guarantee each element of the manifold is unique //
 * for(EntityManager_BoundingVolume* bv = bvhManifold; bv < bvhManifold + bvhManifoldSize; )
 * {
 *     EntityManager_BoundingVolume* bv2 = bvhManifold;
 *     for(; bv2 < bv; bv2++)
 *         if(bv2->entity == bv->entity)
 *             break;
 *     if(bv2 < bv)// if bv is a repeat of a previous BoundingVolume
 *     {
 *         *bv = bvhManifold[--bvhManifoldSize];// decrease the size of the manifold by replacing this entry by the last one
 *         manifoldDuplicates++;
 *     }
 *     else
 *         bv++;// we can move on to check the next BoundingVolume
 * }
 * for(EntityManager_BoundingVolume* bv = bvhManifold; bv < bvhManifold + bvhManifoldSize; bv++)
 *     ...
 * ```
 */
#define KORL_ALGORITHM_BVH_VOLUME_UNION(name) void name(void* a, const void* b)
typedef KORL_ALGORITHM_BVH_VOLUME_UNION(fnSig_korl_algorithm_bvh_volumeUnion);
#define KORL_ALGORITHM_BVH_VOLUME_INTERSECTS(name) bool name(const void* a, const void* b)
typedef KORL_ALGORITHM_BVH_VOLUME_INTERSECTS(fnSig_korl_algorithm_bvh_volumeIntersects);
typedef struct Korl_Algorithm_Bvh_CreateInfo
{
    Korl_Memory_AllocatorHandle           allocator;
    u32                                   leafBoundingVolumes;// we must know up front how many leaf bounding volumes are going to be contained in this BVH
    u32                                   boundingVolumeStride;// set this to the size, in bytes, of the user's leaf bounding volume struct; related to `defaultBoundingVolume`
    u32                                   maxLeafVolumesPerNode;// _must_ be > 0; enforces a capacity of the final # of leaf volumes per BVH node; higher #s should yield faster bvh build times, but larger query arrays; prefer to keep this # as close to 0 as possible; a good first choice is probably like ~4, but I have not yet done any analysis on this and results will vary depending on the model
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
    void*                         nodes;// dynamic array of Korl_Algorithm_Bvh_Node interleaved with a user-defined "volume" struct for each Node; we can't really utilize stb_ds for this unfortunately, because stb_ds needs to know the datatype's size, and we don't actually know what the user defines as a "volume"
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
#define KORL_ALGORITHM_BVH_FOR_EACH_LEAF_NODE_CALLBACK(name) void name(void* userData, u32 leafNodeVolumesStride, const void* leafNodeVolumes, u32 leafNodeVolumesSize)
typedef KORL_ALGORITHM_BVH_FOR_EACH_LEAF_NODE_CALLBACK(korl_algorithm_bvh_forEachLeafNodeCallback);
korl_internal void                korl_algorithm_bvh_forEachLeafNode(Korl_Algorithm_Bvh* context, korl_algorithm_bvh_forEachLeafNodeCallback* callback, void* callbackUserData);
//KORL-ISSUE-000-000-200: algorithm: API rectification; rename Korl_Algorithm_GraphDirected* to Korl_Algorithm_DAG, as this code _only_ deals with _acyclic_ directed graphs
typedef struct Korl_Algorithm_GraphDirected_CreateInfo
{
    Korl_Memory_AllocatorHandle allocator;
    u32                         nodesSize;
    bool                        isArborescence;// if true, korl_algorithm_graphDirected_sortTopological* functions will assert that the graph is either an arborescence or an arborescence forest
} Korl_Algorithm_GraphDirected_CreateInfo;
typedef struct Korl_Algorithm_GraphDirected
{
    Korl_Algorithm_GraphDirected_CreateInfo        createInfo;
    struct _Korl_Algorithm_GraphDirected_NodeMeta* nodeMetas;
    struct _Korl_Algorithm_GraphDirected_Edge*     edges;
    u32                                            edgesCapacity;
    u32                                            edgesSize;
} Korl_Algorithm_GraphDirected;
typedef struct Korl_Algorithm_Bvh_Node
{
    u32 leafBoundingVolumeIndexStart;
    u32 leafBoundingVolumeIndexEnd;
    u32 bvhNodeChildIndexOffsetLeft;
    u32 bvhNodeChildIndexOffsetRight;
    // this struct should be followed by user-defined volume struct, which is where we will store this node's total volume
    //KORL-ISSUE-000-000-154: algorithm/bvh: this is currently a waste of space, as the user-defined volume also includes a pointer back to user-defined struct that is encapsulated by the volume; consider making the user-defined volume struct _just_ be the AABB
} Korl_Algorithm_Bvh_Node;
/** by returning to the user the # of children in each node, & ensuring that the 
 * SortedElements are arranged in such a way that all their sub-trees are 
 * contiguous in memory, the user can perform more advanced iteration techniques 
 * in linear time, such as iterating over each node in any given sub-tree */
typedef struct Korl_Algorithm_GraphDirected_SortedElement
{
    u32 index;// index into the original array of objects the user wants to obtain the topological sort of
    u32 directChildren;// the # of objects whose parent is the object in the original array at `index`
} Korl_Algorithm_GraphDirected_SortedElement;
korl_internal Korl_Algorithm_GraphDirected                korl_algorithm_graphDirected_create(const Korl_Algorithm_GraphDirected_CreateInfo*const createInfo);
korl_internal void                                        korl_algorithm_graphDirected_destroy(Korl_Algorithm_GraphDirected* context);
korl_internal void                                        korl_algorithm_graphDirected_addEdge(Korl_Algorithm_GraphDirected* context, u32 indexParent, u32 indexChild);
/** \param o_sortedElements _must_ be >= \c context->createInfo.nodesSize in length; 
 * see \c korl_algorithm_graphDirected_sortTopologicalNew return value comments 
 * for details on what is stored in this location
 * \return \c true if topological sort succeeds, \c false if a cycle is detected */
korl_internal bool                                        korl_algorithm_graphDirected_sortTopological(Korl_Algorithm_GraphDirected* context, Korl_Algorithm_GraphDirected_SortedElement* o_sortedElements);
/** \return An array of indices into the user's object array sorted 
 * topologically, according to what the user provided to the graph via the 
 * \c korl_algorithm_graphDirected_addEdge function, such that parent node 
 * indices _always_ come _before_ their child node indices.  
 * The resulting array is additionally arranged such that all sub-trees in the 
 * forest are contiguous in memory, which allows the user to easily perform 
 * advanced iteration techniques, such as recursively enumerating over any given 
 * sub-tree in linear time.  If no such order exists (the graph created by the 
 * user contains cycles), then \c NULL is returned.  The result array is 
 * allocated via the \c allocator parameter. */
korl_internal Korl_Algorithm_GraphDirected_SortedElement* korl_algorithm_graphDirected_sortTopologicalNew(Korl_Algorithm_GraphDirected* context, Korl_Memory_AllocatorHandle allocator);
