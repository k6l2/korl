#pragma once
#include "kgtShape.h"
using KgtBodyColliderShapeHandle    = u32;
using KgtBodyColliderBodyHandle     = u64;
using KgtBodyColliderManifoldHandle = u64;
using KgtBodyColliderShapeId    = u16;
using KgtBodyColliderBodyId     = u32;
using KgtBodyColliderManifoldId = u32;
struct KgtBodyColliderBody
{
	KgtBodyColliderBodyHandle handle;
	KgtBodyColliderShapeHandle hShape;
	v3f32 position;
	q32 orient = q32::IDENTITY;
	/* internal manifold resource handling */
	/* all elements of the body's manifold array should possess the SAME 
		HANDLE */
	KgtBodyColliderManifoldHandle hManifoldArray;
	KgtBodyColliderManifoldId manifoldArraySize;
	KgtBodyColliderManifoldId manifoldArrayCapacity;
} FORCE_SYMBOL_EXPORT;
/** A manifold describes data about a collision between two 
 * `KgtBodyColliderBody` objects.  The minimum translation vector described 
 * herein points in the direction from bodyB TOWARDS bodyA.
 */
struct KgtBodyColliderManifold
{
	KgtBodyColliderManifoldHandle handle;
	KgtBodyColliderBodyHandle hBodyA;
	KgtBodyColliderBodyHandle hBodyB;
	/** points in the direction from bodyB TOWARDS bodyA */
	v3f32 minTranslateNormal;
	f32   minTranslateDistance;
	v3f32 worldContactPoints[4];
	u8    worldContactPointsSize;
} FORCE_SYMBOL_EXPORT;
struct KgtBodyColliderMemoryRequirements
{
	KgtBodyColliderShapeId    maxShapes;
	KgtBodyColliderBodyId     maxBodies;
	KgtBodyColliderManifoldId maxCollisionManifolds;
};
struct KgtBodyColliderPoolSlot;
struct KgtBodyCollider
{
	/* internal storage pool addresses */
	KgtBodyColliderPoolSlot* shapeSlots;
	KgtShape*                shapePool;
	KgtBodyColliderPoolSlot* bodySlots;
	KgtBodyColliderBody*     bodyPool;
	KgtBodyColliderPoolSlot* manifoldSlots;
	KgtBodyColliderManifold* manifoldPool;
	/* internal storage pool configuration */
	KgtBodyColliderMemoryRequirements memoryReqs;
	/* internal storage pool driver state */
	KgtBodyColliderShapeId shapeAllocCount;
	KgtBodyColliderShapeId shapeAllocNext;
	KgtBodyColliderBodyId bodyAllocCount;
	KgtBodyColliderBodyId bodyAllocNext;
	KgtBodyColliderManifoldId manifoldAllocCount;
	KgtBodyColliderManifoldId manifoldAllocNext;
};
struct KgtBodyColliderManifoldIterator
{
	KgtBodyColliderManifold* manifold;
	/* internal iteration state */
	KgtBodyCollider* kbc;
	KgtBodyColliderBodyId bodyIndex;
	KgtBodyColliderManifoldId manifoldArrayId;
//	KgtBodyColliderManifoldId visitedManifoldCount;
public:
	operator bool() const;
	KgtBodyColliderManifoldIterator& operator++();
};
internal size_t 
	kgtBodyColliderRequiredBytes(
		const KgtBodyColliderMemoryRequirements& memReqs);
internal KgtBodyCollider* 
	kgtBodyColliderConstruct(
		void* address, const KgtBodyColliderMemoryRequirements& memReqs);
internal KgtBodyColliderShapeHandle 
	kgtBodyColliderAddShapeSphere(KgtBodyCollider* bc, f32 radius);
internal KgtBodyColliderBody* 
	kgtBodyColliderAddBody(
		KgtBodyCollider* bc, KgtBodyColliderShapeHandle* hBcs);
internal void 
	kgtBodyColliderRemoveBody(
		KgtBodyCollider* bc, KgtBodyColliderBodyHandle* hBcb);
internal KgtBodyColliderBody* 
	kgtBodyColliderGetBody(
		KgtBodyCollider* bc, KgtBodyColliderBodyHandle* hBcb);
internal void 
	kgtBodyColliderUpdateManifolds(
		KgtBodyCollider* bc, KgtAllocatorHandle hKal);
internal KgtBodyColliderManifoldIterator 
	kgtBodyColliderGetManifoldIterator(KgtBodyCollider* bc);
/** @return the # of vertices output to `o_vertexData` */
internal size_t 
	kgtBodyColliderGenerateDrawLinesBuffer(
		KgtBodyCollider* bc, void* o_vertexData, size_t vertexDataBytes, 
		size_t vertexByteStride, size_t vertexOffsetPositionV3f32, 
		size_t vertexOffsetColorV4f32);