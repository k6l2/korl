#include "kgtBodyCollider.h"
/* for custom stb_ds implementation */
#include "kgtGameState.h"
internal void 
	kgtBodyColliderMemoryRequirements(
		const KgtBodyColliderMemoryRequirements& memReqs, 
		size_t* o_requiredBytes)
{
	*o_requiredBytes = sizeof(KgtBodyCollider) + 
		memReqs.maxShapes*(sizeof(KgtBodyColliderPoolSlot) + sizeof(KgtShape)) + 
		memReqs.maxBodies*(sizeof(KgtBodyColliderPoolSlot) + 
		                   sizeof(KgtBodyColliderBody)) + 
		memReqs.maxCollisionManifolds*(sizeof(KgtBodyColliderPoolSlot) + 
		                               sizeof(KgtBodyColliderManifold));
}
internal KgtBodyCollider* 
	kgtBodyColliderConstruct(
		void* address, const KgtBodyColliderMemoryRequirements& memReqs)
{
	KgtBodyCollider* result = reinterpret_cast<KgtBodyCollider*>(address);
	*result = {};
	result->shapeSlots = reinterpret_cast<KgtBodyColliderPoolSlot*>(
		reinterpret_cast<u8*>(address) + sizeof(KgtBodyCollider));
	result->shapePool = reinterpret_cast<KgtShape*>(
		reinterpret_cast<u8*>(result->shapeSlots) + 
		memReqs.maxShapes*sizeof(KgtBodyColliderPoolSlot));
	result->bodySlots = reinterpret_cast<KgtBodyColliderPoolSlot*>(
		reinterpret_cast<u8*>(result->shapePool) + 
		memReqs.maxShapes*sizeof(KgtShape));
	result->bodyPool = reinterpret_cast<KgtBodyColliderBody*>(
		reinterpret_cast<u8*>(result->bodySlots) + 
		memReqs.maxBodies*sizeof(KgtBodyColliderPoolSlot));
	result->manifoldSlots = reinterpret_cast<KgtBodyColliderPoolSlot*>(
		reinterpret_cast<u8*>(result->bodyPool) + 
		memReqs.maxBodies*sizeof(KgtBodyColliderBody));
	result->manifoldPool = reinterpret_cast<KgtBodyColliderManifold*>(
		reinterpret_cast<u8*>(result->manifoldSlots) + 
		memReqs.maxCollisionManifolds*sizeof(KgtBodyColliderPoolSlot));
	result->memoryReqs = memReqs;
	memset(result->shapeSlots, 0, 
	       memReqs.maxShapes*sizeof(KgtBodyColliderPoolSlot));
	memset(result->bodySlots, 0, 
	       memReqs.maxBodies*sizeof(KgtBodyColliderPoolSlot));
	memset(result->manifoldSlots, 0, 
	       memReqs.maxCollisionManifolds*sizeof(KgtBodyColliderPoolSlot));
	return result;
}
/** combine the 16-bit shape Id with the 16-bit slot salt into a 32-bit 
 * handle */
internal KgtBodyColliderShapeHandle 
	kgtBodyColliderMakeShapeHandle(
		KgtBodyCollider* bc, KgtBodyColliderShapeId sid)
{
	kassert(sid < 0xFFFF - 1);
	return (u32(sid + 1) << 16) | bc->shapeSlots[sid].salt;
}
/** @return true if the param `hs` is valid */
internal bool 
	kgtBodyColliderParseShapeHandle(
		KgtBodyColliderShapeHandle hs, 
		KgtBodyColliderShapeId* o_sid, u16* o_salt)
{
	const u16 rawId = hs >> 16;
	if(rawId == 0)
		return false;
	*o_sid  = static_cast<KgtBodyColliderShapeId>(rawId - 1);
	*o_salt = hs & 0xFFFF;
	return true;
}
internal KgtBodyColliderShapeHandle 
	kgtBodyColliderAddShapeSphere(KgtBodyCollider* bc, f32 radius)
{
	if(bc->shapeAllocCount >= bc->memoryReqs.maxShapes)
	{
		KLOG(ERROR, "Shape pool overflow!");
		return 0;
	}
	/* set bc->shapeAllocNext to be the next available unoccupied pool index */
	bool shapeIdFound = false;
	for(KgtBodyColliderShapeId sid = 0; sid < bc->memoryReqs.maxShapes; sid++)
	{
		const KgtBodyColliderShapeId sidMod = kmath::safeTruncateU16(
			(bc->shapeAllocNext + sid) % bc->memoryReqs.maxShapes);
		if(!bc->shapeSlots[sidMod].occupied)
		{
			bc->shapeAllocNext = sidMod;
			shapeIdFound = true;
			break;
		}
	}
	if(!shapeIdFound)
	{
		KLOG(ERROR, "Failed to find next unoccupied shape!");
		return 0;
	}
	/* initialize the shape in the pool */
	bc->shapePool[bc->shapeAllocNext].sphere = 
		{ .type = KgtShapeType::SPHERE
		, .radius = radius };
	/* mark the slot as occupied */
	bc->shapeSlots[bc->shapeAllocNext].occupied = true;
	bc->shapeSlots[bc->shapeAllocNext].salt++;
	/* update the allocation counter */
	bc->shapeAllocCount++;
	/* generate & return the shape handle */
	return kgtBodyColliderMakeShapeHandle(bc, bc->shapeAllocNext);
}
/** combine the 32-bit body Id with the 16-bit slot salt into a 64-bit 
 * handle */
internal KgtBodyColliderBodyHandle 
	kgtBodyColliderMakeBodyHandle(
		KgtBodyCollider* bc, KgtBodyColliderBodyId bid)
{
	kassert(bid < 0xFFFFFFFF - 1);
	return (u32(bid + 1) << 16) | bc->bodySlots[bid].salt;
}
internal KgtBodyColliderBody* 
	kgtBodyColliderAddBody(
		KgtBodyCollider* bc, KgtBodyColliderShapeHandle* hBcs)
{
	/* ensure we logically have room for more bodies */
	if(bc->bodyAllocCount >= bc->memoryReqs.maxBodies)
	{
		KLOG(ERROR, "Body pool overflow!");
		return nullptr;
	}
	/* validate the shape handle */
	KgtBodyColliderShapeId sid;
	u16 shapeSalt;
	if(   !kgtBodyColliderParseShapeHandle(*hBcs, &sid, &shapeSalt)
	   || !bc->shapeSlots[sid].occupied
	   ||  bc->shapeSlots[sid].salt != shapeSalt)
	{
		KLOG(ERROR, "Attempted to create a body with an invalid shape "
		     "handle (%i)!", *hBcs);
		*hBcs = 0;
		return nullptr;
	}
	/* set bc->bodyAllocNext to be the index of the unoccupied body to use */
	bool bodyIdFound = false;
	for(KgtBodyColliderBodyId bid = 0; bid < bc->memoryReqs.maxBodies; bid++)
	{
		const KgtBodyColliderBodyId bidMod = 
			(bc->bodyAllocNext + bid) % bc->memoryReqs.maxBodies;
		if(!bc->bodySlots[bidMod].occupied)
		{
			bc->bodyAllocNext = bidMod;
			bodyIdFound = true;
			break;
		}
	}
	if(!bodyIdFound)
	{
		KLOG(ERROR, "Failed to find next unoccupied body!");
		return nullptr;
	}
	/* initialize & allocate the new body in the body pool */
	bc->bodySlots[bc->bodyAllocNext].occupied = true;
	bc->bodySlots[bc->bodyAllocNext].salt++;
	bc->bodyPool[bc->bodyAllocNext] = {};
	bc->bodyPool[bc->bodyAllocNext].hShape = *hBcs;
	bc->bodyPool[bc->bodyAllocNext].handle = 
		kgtBodyColliderMakeBodyHandle(bc, bc->bodyAllocNext);
	bc->bodyAllocCount++;
	return &bc->bodyPool[bc->bodyAllocNext];
}
/** @return true if the param `hb` is valid */
internal bool 
	kgtBodyColliderParseBodyHandle(
		KgtBodyColliderBodyHandle hb, 
		KgtBodyColliderBodyId* o_bid, u16* o_salt)
{
	const u64 rawId = hb >> 16;
	if(rawId == 0)
		return false;
	*o_bid  = static_cast<KgtBodyColliderBodyId>(rawId - 1);
	*o_salt = hb & 0xFFFF;
	return true;
}
/** @return true if the param `hm` is valid */
internal bool 
	kgtBodyColliderParseManifoldHandle(
		KgtBodyColliderManifoldHandle hm, 
		KgtBodyColliderManifoldId* o_mid, u16* o_salt)
{
	const u64 rawId = hm >> 16;
	if(rawId == 0)
		return false;
	*o_mid  = static_cast<KgtBodyColliderManifoldId>(rawId - 1);
	*o_salt = hm & 0xFFFF;
	return true;
}
internal void 
	kgtBodyColliderBodyReleaseManifolds(
		KgtBodyCollider* bc, KgtBodyColliderBody* body)
{
	if(!body->hManifoldArray)
		return;
	KgtBodyColliderManifoldId mid;
	u16 manifoldArraySalt;
	if(!kgtBodyColliderParseManifoldHandle(
		   body->hManifoldArray, &mid, &manifoldArraySalt)
		|| bc->manifoldSlots[mid].salt != manifoldArraySalt)
	{
		KLOG(ERROR, "Invalid manifold array! handle=0x%li", 
		     body->hManifoldArray);
	}
	kassert(mid + body->manifoldArrayCapacity <= 
	            bc->memoryReqs.maxCollisionManifolds);
	for(KgtBodyColliderManifoldId m = mid; 
		m < mid + body->manifoldArrayCapacity; m++)
	{
		bc->manifoldSlots[m].occupied = false;
	}
	bc->manifoldAllocCount -= body->manifoldArrayCapacity;
	body->hManifoldArray        = 0;
	body->manifoldArrayCapacity = 0;
	body->manifoldArraySize     = 0;
}
internal void 
	kgtBodyColliderRemoveBody(
		KgtBodyCollider* bc, KgtBodyColliderBodyHandle* hBcb)
{
	defer(*hBcb = 0);
	KgtBodyColliderBodyId bid;
	u16 bodySalt;
	if(!kgtBodyColliderParseBodyHandle(*hBcb, &bid, &bodySalt)
		|| bc->bodySlots[bid].salt != bodySalt)
	{
		return;
	}
	if(bc->bodyPool[bid].hManifoldArray)
	{
		kgtBodyColliderBodyReleaseManifolds(bc, &bc->bodyPool[bid]);
	}
	bc->bodySlots[bid].occupied = false;
	bc->bodyAllocCount--;
}
internal KgtShape*
	kgtBodyColliderGetShape(
		KgtBodyCollider* bc, KgtBodyColliderShapeHandle* hBcs)
{
	KgtBodyColliderShapeId sid;
	u16 shapeSalt;
	if(   !kgtBodyColliderParseShapeHandle(*hBcs, &sid, &shapeSalt)
	   || !bc->shapeSlots[sid].occupied
	   ||  bc->shapeSlots[sid].salt != shapeSalt)
	{
		*hBcs = 0;
		return nullptr;
	}
	return &bc->shapePool[sid];
}
global_variable const u32 
	KGT_BODY_COLLIDER_CIRCLE_SPHERE_LATITUDE_SEGMENTS  = 8;
global_variable const u32 
	KGT_BODY_COLLIDER_CIRCLE_SPHERE_LONGITUDE_SEGMENTS = 8;
internal size_t 
	kgtBodyColliderShapeVertexCount(const KgtShape* shape)
{
	switch(shape->type)
	{
		case KgtShapeType::SPHERE:{
			return kmath::generateMeshCircleSphereWireframeVertexCount(
				KGT_BODY_COLLIDER_CIRCLE_SPHERE_LATITUDE_SEGMENTS, 
				KGT_BODY_COLLIDER_CIRCLE_SPHERE_LONGITUDE_SEGMENTS);
		}break;
		case KgtShapeType::BOX:
		default:
			KLOG(ERROR, "NOT IMPLEMENTED!");
		break;
	}
	return 0;
}
internal void 
	kgtBodyColliderEmitBodyWireframe(
		KgtBodyColliderBody* body, const KgtShape* shape, 
		size_t shapeVertexCount, size_t vertexByteStride, u8*const o_positions, 
		u8*const o_colors, const v4f32& color)
{
	const size_t vertexDataBytes = vertexByteStride*shapeVertexCount;
	for(size_t v = 0; v < shapeVertexCount; v++)
	{
		v4f32*const o_color = reinterpret_cast<v4f32*>(
			o_colors + v*vertexByteStride);
		*o_color = color;
	}
	/* generate the vertex positions in model-space */
	switch(shape->type)
	{
		case KgtShapeType::SPHERE:{
			kmath::generateMeshCircleSphereWireframe(
				shape->sphere.radius, 
				KGT_BODY_COLLIDER_CIRCLE_SPHERE_LATITUDE_SEGMENTS, 
				KGT_BODY_COLLIDER_CIRCLE_SPHERE_LONGITUDE_SEGMENTS, o_positions, 
				vertexDataBytes, vertexByteStride, 0);
		}break;
		case KgtShapeType::BOX:
		default:
			KLOG(ERROR, "NOT IMPLEMENTED!");
		break;
	}
	/* transform the vertex positions to world-space using `body` */
	for(size_t v = 0; v < shapeVertexCount; v++)
	{
		v3f32*const o_position = reinterpret_cast<v3f32*>(
			o_positions + v*vertexByteStride);
		*o_position = body->orient.transform(*o_position) + body->position;
	}
}
internal size_t 
	kgtBodyColliderManifoldVertexCount(
		KgtBodyCollider* bc, KgtBodyColliderManifoldId m)
{
	kassert(!"TODO");
	return 0;
}
internal void 
	kgtBodyColliderEmitManifoldWireframe(
		KgtBodyCollider* bc, KgtBodyColliderManifoldId m, 
		size_t vertexByteStride, u8*const o_positions, u8*const o_colors)
{
	/* iterate over each manifold:
		iterate over each contact point:
			- draw a line starting at the contact point ending at the MTV 
				relative to that point
			- color the line vertices cyan-magenta so we can tell where the line 
				begins and in what direction it is going */
	///@TODO
	kassert(!"TODO");
}
internal size_t 
	kgtBodyColliderGenerateDrawLinesBuffer(
		KgtBodyCollider* bc, void* o_vertexData, size_t vertexDataBytes, 
		size_t vertexByteStride, size_t vertexOffsetPositionV3f32, 
		size_t vertexOffsetColorV4f32)
{
	size_t vertexCount = 0;
	const size_t maxVertexCount = vertexDataBytes / vertexByteStride;
	u8*const o_vertexDataU8 = reinterpret_cast<u8*>(o_vertexData);
	u8* o_positions = o_vertexDataU8 + vertexOffsetPositionV3f32;
	u8* o_colors    = o_vertexDataU8 + vertexOffsetColorV4f32;
	/* sizeof(position) + sizeof(color) */
	const size_t requiredVertexBytes = sizeof(v3f32) + sizeof(v4f32);
	/* make some reasonable sanity checks about the output vertex data 
		specifications */
	kassert(vertexByteStride >= requiredVertexBytes);
	kassert(vertexOffsetPositionV3f32 <= vertexByteStride - sizeof(v3f32));
	kassert(vertexOffsetColorV4f32    <= vertexByteStride - sizeof(v4f32));
	/* iterate over each body:
		- generate a wireframe mesh for the appropriate shape
		- perform a model=>world space transformation on the vertices
		- color the vertices based on their collision state:
			- white == no collision
			- red == collision */
	{
		KgtBodyColliderBodyId bCount = 0;
		for(KgtBodyColliderBodyId b = 0; 
			   b < bc->memoryReqs.maxBodies 
			&& bCount < bc->bodyAllocCount; b++)
		{
			if(!bc->bodySlots[b].occupied)
				continue;
			/* get the shape of the body reliably */
			KgtShape*const shape = 
				kgtBodyColliderGetShape(bc, &bc->bodyPool[b].hShape);
			kassert(shape);
			/* color the vertices based on collision state */
			bool atLeastOneContactingManifold = false;
			if(bc->bodyPool[b].hManifoldArray)
			{
				KgtBodyColliderManifoldId mid;
				u16 manifoldSalt;
				if(!kgtBodyColliderParseManifoldHandle(
						bc->bodyPool[b].hManifoldArray, &mid, &manifoldSalt)
					|| bc->manifoldSlots[mid].salt != manifoldSalt)
				{
					KLOG(ERROR, "Invalid manifold handle (%li)!", 
					     bc->bodyPool[b].hManifoldArray);
				}
				for(KgtBodyColliderManifoldId m = mid; 
					m < mid + bc->bodyPool[b].manifoldArraySize; m++)
				{
					if(bc->manifoldPool[m].worldContactPointsSize <= 0)
						continue;
					atLeastOneContactingManifold = true;
					/* emit some line segments representing the collision 
						manifold */
					const size_t manifoldVertexCount = 
						kgtBodyColliderManifoldVertexCount(bc, m);
					kassert(manifoldVertexCount > 0);
					if(vertexCount + manifoldVertexCount > maxVertexCount)
					{
						KLOG(WARNING, "Supplied vertex buffer array size (%i) "
						     "is not large enough!", maxVertexCount);
						return vertexCount;
					}
					kgtBodyColliderEmitManifoldWireframe(
						bc, m, vertexByteStride, o_positions, o_colors);
					o_positions += manifoldVertexCount*vertexByteStride;
					o_colors    += manifoldVertexCount*vertexByteStride;
					vertexCount += manifoldVertexCount;
				}
			}
			const v4f32 color = (bc->bodyPool[b].hManifoldArray 
				? (atLeastOneContactingManifold
					? v4f32{1,0,0,1} 
					: v4f32{1,1,0,1}) 
				: v4f32{1,1,1,1});
			/* output the wireframe representation of the shape */
			const size_t shapeVertexCount = 
				kgtBodyColliderShapeVertexCount(shape);
			if(vertexCount + shapeVertexCount > maxVertexCount)
			{
				KLOG(WARNING, "Supplied vertex buffer array size (%i) is not "
				     "large enough!", maxVertexCount);
				return vertexCount;
			}
			kgtBodyColliderEmitBodyWireframe(
				&bc->bodyPool[b], shape, shapeVertexCount, vertexByteStride, 
				o_positions, o_colors, color);
			o_positions += shapeVertexCount*vertexByteStride;
			o_colors    += shapeVertexCount*vertexByteStride;
			vertexCount += shapeVertexCount;
			bCount++;
		}
		kassert(bCount == bc->bodyAllocCount);
	}
	return vertexCount;
}
struct KgtBodyColliderBodyAabb
{
	v3f32 min;
	v3f32 max;
};
internal KgtBodyColliderBodyAabb 
	kgtBodyColliderBodyGetAabb(KgtBodyCollider* bc, KgtBodyColliderBody* body)
{
	KgtBodyColliderBodyAabb result = {};
	KgtShape*const shape = kgtBodyColliderGetShape(bc, &body->hShape);
	kassert(shape);
	kgtShapeCalculateAabb(
		*shape, &result.min, &result.max, body->position, body->orient);
	return result;
}
internal bool 
	kgtBodyColliderBodyAabbsAreColliding(
		KgtBodyCollider* bc, KgtBodyColliderBody* b0, KgtBodyColliderBody* b1)
{
	const KgtBodyColliderBodyAabb aabb0 = kgtBodyColliderBodyGetAabb(bc, b0);
	const KgtBodyColliderBodyAabb aabb1 = kgtBodyColliderBodyGetAabb(bc, b1);
	return aabb0.min.x <= aabb1.max.x && aabb0.max.x >= aabb1.min.x 
	    && aabb0.min.y <= aabb1.max.y && aabb0.max.y >= aabb1.min.y 
	    && aabb0.min.z <= aabb1.max.z && aabb0.max.z >= aabb1.min.z;
}
#define KGT_BODY_COLLIDER_MANIFOLD_SOLVER_FUNCTION(name) \
	void name(\
		KgtBodyColliderManifold* o_manifold, \
		const KgtShape* shape0, const KgtShape* shape1, \
		KgtBodyColliderBody* b0, KgtBodyColliderBody* b1)
typedef KGT_BODY_COLLIDER_MANIFOLD_SOLVER_FUNCTION(
	KgtBodyColliderManifoldSolverFunction);
internal KGT_BODY_COLLIDER_MANIFOLD_SOLVER_FUNCTION(
	kgtBodyColliderMsf_S_S)
{
	kassert(shape0->type == KgtShapeType::SPHERE);
	kassert(shape1->type == KgtShapeType::SPHERE);
	v3f32 b1ToB0         = b0->position - b1->position;
	const f32 distance   = b1ToB0.normalize();
	const f32 sumOfRadii = shape0->sphere.radius + shape1->sphere.radius;
	const bool colliding = distance < sumOfRadii;
	if(colliding)
	{
		local_persist const v3f32 DEGENERATE_NORMAL = v3f32::Z;
		if(kmath::isNearlyZero(distance))
			o_manifold->minTranslateNormal = DEGENERATE_NORMAL;
		else
			o_manifold->minTranslateNormal = b1ToB0;
		o_manifold->minTranslateDistance   = sumOfRadii - distance;
		o_manifold->worldContactPointsSize = 1;
		o_manifold->worldContactPoints[0]  = 
			b1->position + 0.5f*distance*b1ToB0;
	}
	else
	{
		o_manifold->worldContactPointsSize = 0;
	}
}
/** combine the 32-bit manifold Id with the 16-bit slot salt into a 64-bit 
 * handle */
internal KgtBodyColliderManifoldHandle 
	kgtBodyColliderMakeManifoldHandle(
		KgtBodyCollider* bc, KgtBodyColliderManifoldId mid)
{
	kassert(mid < 0xFFFFFFFF - 1);
	return (u64(mid + 1) << 16) | bc->manifoldSlots[mid].salt;
}
internal void 
	kgtBodyColliderAllocManifolds(
		KgtBodyCollider* bc, KgtBodyColliderBody* body, 
		KgtBodyColliderManifoldId manifoldCount)
{
	kassert(bc->memoryReqs.maxCollisionManifolds >= 
	            bc->manifoldAllocCount + manifoldCount);
	/* search for a contiguous array of unoccupied manifold pool slots */
	KgtBodyColliderManifoldId mid = bc->memoryReqs.maxCollisionManifolds;
	for(KgtBodyColliderManifoldId m = 0; 
		m < bc->memoryReqs.maxCollisionManifolds; m++)
	{
		const KgtBodyColliderManifoldId mMod = 
			(bc->manifoldAllocNext + m) % bc->memoryReqs.maxCollisionManifolds;
		/* if the current modulus position in the pool is incapable of storing 
			`manifoldCount` elements, then we need to move `m` forward by an 
			amount that will bring `mMod` back to 0 on the next iteration */
		//           X  N
		// [0][1][2][3][4]
		if(mMod > bc->memoryReqs.maxCollisionManifolds - manifoldCount)
		{
			const KgtBodyColliderManifoldId mAdvance = 
				bc->memoryReqs.maxCollisionManifolds - mMod - 1;
			kassert(mAdvance > 0);
			m += mAdvance;
			continue;
		}
		if(bc->manifoldSlots[mMod].occupied)
			continue;
		/* check to see if (`manifoldCount` - 1) unoccupied slots exist after 
			`mMod` */
		KgtBodyColliderManifoldId unoccupiedCount = 1;
		for(KgtBodyColliderManifoldId mu = mMod + 1; 
			mu < mMod + manifoldCount; mu++)
		{
			if(bc->manifoldSlots[mMod].occupied)
				break;
			unoccupiedCount++;
		}
		if(unoccupiedCount < manifoldCount)
			continue;
		mid = mMod;
		break;
	}
	if(mid >= bc->memoryReqs.maxCollisionManifolds)
		KLOG(ERROR, "Unable to allocate collision manifolds!");
	/* at this point we have a valid location to allocate an array of manifolds 
		in the manifold pool, so let's initialize them and update allocation 
		book keeping */
	bc->manifoldSlots[mid].occupied = true;
	bc->manifoldSlots[mid].salt++;
	const KgtBodyColliderManifoldHandle hManifoldArray = 
		kgtBodyColliderMakeManifoldHandle(bc, mid);
	for(KgtBodyColliderManifoldId m = mid; m < mid + manifoldCount; m++)
	/* make sure that all the manifolds in the array have the SAME HANDLE */
	{
		bc->manifoldSlots[m].occupied = true;
		bc->manifoldSlots[m].salt     = bc->manifoldSlots[mid].salt;
		bc->manifoldPool[m].handle = hManifoldArray;
	}
	body->manifoldArrayCapacity = manifoldCount;
	body->manifoldArraySize     = 0;
	body->hManifoldArray        = hManifoldArray;
	bc->manifoldAllocCount += manifoldCount;
	bc->manifoldAllocNext   = mid + manifoldCount;
}
internal void 
	kgtBodyColliderReallocManifolds(
		KgtBodyCollider* bc, KgtBodyColliderBody* body, 
		KgtBodyColliderManifoldId manifoldCount)
{
	if(!body->hManifoldArray)
	{
		kgtBodyColliderAllocManifolds(bc, body, manifoldCount);
		return;
	}
	if(body->manifoldArrayCapacity >= manifoldCount)
		return;
	/* first we need to check to see if we can just expand the manifold array 
		in-place in the pool */
	KgtBodyColliderManifoldId mid;
	u16 manifoldArraySalt;
	if(!kgtBodyColliderParseManifoldHandle(
		   body->hManifoldArray, &mid, &manifoldArraySalt)
		|| bc->manifoldSlots[mid].salt != manifoldArraySalt)
	{
		KLOG(ERROR, "Invalid manifold array! handle=0x%li", 
		     body->hManifoldArray);
	}
	//        X     N
	// [0][1][2][3][4]
	/* first new allocation requirement; if the new `manifoldCount` would make 
		our array go past the bounds of `bc->memoryReqs.maxCollisionManifolds */
	bool newAllocationRequired = 
		(mid + manifoldCount >= bc->memoryReqs.maxCollisionManifolds);
	if(!newAllocationRequired)
	/* if `manifoldCount` will let us stay within the bounds of the manifold 
		pool, we can check for the next condition for new allocation 
		requirement; if there are occupied manifolds in the desired grow 
		region */
	{
		for(KgtBodyColliderManifoldId m = mid + body->manifoldArrayCapacity; 
			m < mid + manifoldCount; m++)
		{
			if(bc->manifoldSlots[m].occupied)
			{
				newAllocationRequired = true;
				break;
			}
		}
	}
	if(newAllocationRequired)
	/* alloc -> copy -> free.  This will probably happen most of the time... */
	{
		KgtBodyColliderBody bodyOld = *body;
		/* create a new allocation */
		kgtBodyColliderAllocManifolds(bc, body, manifoldCount);
		/* copy the manifolds from the old allocation */
		KgtBodyColliderManifoldId midNew;
		u16 manifoldArraySaltNew;
		if(!kgtBodyColliderParseManifoldHandle(
			   body->hManifoldArray, &midNew, &manifoldArraySaltNew)
			|| bc->manifoldSlots[midNew].salt != manifoldArraySaltNew)
		{
			KLOG(ERROR, "Invalid manifold array! handle=0x%li", 
			     body->hManifoldArray);
		}
		for(KgtBodyColliderManifoldId m = 0; m < bodyOld.manifoldArraySize; m++)
		{
			bc->manifoldPool[midNew + m] = bc->manifoldPool[mid + m];
		}
		/* update the new allocation's handles, since they are overwritten by 
			the old manifolds' data */
		for(KgtBodyColliderManifoldId m = 0; 
			m < body->manifoldArrayCapacity; m++)
		{
			bc->manifoldPool[midNew + m].handle = body->hManifoldArray;
		}
		/* erase the old allocation */
		kgtBodyColliderBodyReleaseManifolds(bc, &bodyOld);
	}
	else
	/* simply expand the manifold array in-place */
	{
		for(KgtBodyColliderManifoldId m = mid + body->manifoldArrayCapacity; 
			m < mid + manifoldCount; m++)
		{
			bc->manifoldSlots[m].occupied = true;
			bc->manifoldSlots[m].salt     = bc->manifoldSlots[mid].salt;
			bc->manifoldPool[m].handle = bc->manifoldPool[mid].handle;
		}
		bc->manifoldAllocCount += manifoldCount - body->manifoldArrayCapacity;
		body->manifoldArrayCapacity = manifoldCount;
	}
}
internal void 
	kgtBodyColliderBodyAddManifold(
		KgtBodyCollider* bc, KgtBodyColliderBody* b, KgtBodyColliderManifold m)
{
	/* first, we need to ensure that the manifold array can hold another 
		manifold */
	if(!b->hManifoldArray)
	{
		kgtBodyColliderAllocManifolds(bc, b, 8);
	}
	else if(b->manifoldArraySize >= b->manifoldArrayCapacity)
	{
		kgtBodyColliderReallocManifolds(bc, b, 2*b->manifoldArrayCapacity);
	}
	/* add the new manifold to the body's manifold array */
	{
		KgtBodyColliderManifoldId mid;
		u16 manifoldArraySalt;
		if(!kgtBodyColliderParseManifoldHandle(
				b->hManifoldArray, &mid, &manifoldArraySalt)
			|| bc->manifoldSlots[mid].salt != manifoldArraySalt)
		{
			KLOG(ERROR, "Invalid manifold array! handle=0x%li", 
			     b->hManifoldArray);
		}
		m.handle = b->hManifoldArray;
		bc->manifoldPool[mid + b->manifoldArraySize++] = m;
	}
}
internal void 
	kgtBodyColliderMakeManifold(
		KgtBodyCollider* bc, KgtBodyColliderBody* b0, KgtBodyColliderBody* b1)
{
	const KgtShape*const shape0 = kgtBodyColliderGetShape(bc, &b0->hShape);
	const KgtShape*const shape1 = kgtBodyColliderGetShape(bc, &b1->hShape);
	kassert(shape0 && shape1);
	KgtBodyColliderManifold manifold = {};
	manifold.hBodyA = b0->handle;
	manifold.hBodyB = b1->handle;
	local_persist const u8 SHAPE_TYPE_COUNT = 1;
	local_persist KgtBodyColliderManifoldSolverFunction*const 
		SOLVERS[SHAPE_TYPE_COUNT][SHAPE_TYPE_COUNT] = 
			{ { kgtBodyColliderMsf_S_S } };
	kassert(u8(shape0->type) <= SHAPE_TYPE_COUNT);
	kassert(u8(shape1->type) <= SHAPE_TYPE_COUNT);
	SOLVERS[u8(shape0->type)][u8(shape1->type)](
		&manifold, shape0, shape1, b0, b1);
	kgtBodyColliderBodyAddManifold(bc, b0, manifold);
}
internal void 
	kgtBodyColliderUpdateManifolds(
		KgtBodyCollider* bc, KgtAllocatorHandle hKal)
{
	/* @TODO(speed): use some sort of space-partitioning acceleration structure 
		to reduce the # of iterations */
	KgtBodyColliderBody** bodyPointerArray = 
		arrinit(KgtBodyColliderBody*, hKal);
	arrsetcap(bodyPointerArray, bc->bodyAllocCount);
	defer(arrfree(bodyPointerArray));
	/* gather a contiguous list of bodies in the body collider, and while doing 
		so check for collisions with the current body */
	{
		size_t bCount = 0;
		for(size_t b = 0; 
			b < bc->memoryReqs.maxBodies && bCount < bc->bodyAllocCount; b++)
		{
			if(!bc->bodySlots[b].occupied)
				continue;
			bc->bodyPool[b].manifoldArraySize = 0;
			for(size_t b1 = 0; b1 < arrlenu(bodyPointerArray); b1++)
			{
				if(!kgtBodyColliderBodyAabbsAreColliding(
						bc, &bc->bodyPool[b], bodyPointerArray[b1]))
					continue;
				kgtBodyColliderMakeManifold(
					bc, &bc->bodyPool[b], bodyPointerArray[b1]);
				kgtBodyColliderMakeManifold(
					bc, bodyPointerArray[b1], &bc->bodyPool[b]);
			}
			if(bc->bodyPool[b].manifoldArraySize == 0)
			{
				kgtBodyColliderBodyReleaseManifolds(bc, &bc->bodyPool[b]);
			}
			arrpush(bodyPointerArray, &bc->bodyPool[b]);
			bCount++;
		}
		kassert(bCount == bc->bodyAllocCount);
	}
	/* for safety, verify the integrity of our memory pools */
	{
		KgtBodyColliderManifoldId manifoldTotalOccupied = 0;
		for(KgtBodyColliderManifoldId m = 0; 
			m < bc->memoryReqs.maxCollisionManifolds; m++)
		{
			if(bc->manifoldSlots[m].occupied)
				manifoldTotalOccupied++;
		}
		kassert(manifoldTotalOccupied == bc->manifoldAllocCount);
		KgtBodyColliderManifoldId manifoldTotalCapacity = 0;
		size_t bCount = 0;
		for(size_t b = 0; 
			b < bc->memoryReqs.maxBodies && bCount < bc->bodyAllocCount; b++)
		{
			if(!bc->bodySlots[b].occupied)
				continue;
			manifoldTotalCapacity += bc->bodyPool[b].manifoldArrayCapacity;
			bCount++;
		}
		kassert(manifoldTotalCapacity == bc->manifoldAllocCount);
	}
}
internal KgtBodyColliderBody* 
	kgtBodyColliderGetBody(
		KgtBodyCollider* bc, KgtBodyColliderBodyHandle* hBcb)
{
	KgtBodyColliderBodyId bid;
	u16 bodySalt;
	if(!kgtBodyColliderParseBodyHandle(*hBcb, &bid, &bodySalt)
		|| bc->bodySlots[bid].salt != bodySalt)
	{
		*hBcb = 0;
		return nullptr;
	}
	return &bc->bodyPool[bid];
}
KgtBodyColliderManifoldIterator::operator bool() const
{
	return manifold != nullptr;
}
KgtBodyColliderManifoldIterator& 
	KgtBodyColliderManifoldIterator::operator++()
{
#if 0// this wont work because I don't currently keep track of COLLIDING manifolds
	visitedManifoldCount++;
	if(visitedManifoldCount >= kbc->manifoldAllocCount)
	/* early escape case; we've visited all the manifolds */
	{
		manifold = nullptr;
		/* @TODO(speed): move the memory pool integrity check code from 
			`kgtBodyColliderUpdateManifolds` into here to eliminate at least one 
			unnecessary loop through the pools */
	}
	else
#endif// 0
	/* so actually, in order to loop over all the ACTIVE collision 
		manifolds, we MUST loop over all the bodies, because each body 
		stores the value of how many manifold array elements they are 
		attached to */
	bool firstBody = true;
	for(KgtBodyColliderBodyId bid = bodyIndex; 
		bid < kbc->memoryReqs.maxBodies; bid++)
	{
		if(!kbc->bodySlots[bid].occupied)
			continue;
		KgtBodyColliderBody*const body = &kbc->bodyPool[bid];
		if(!body->hManifoldArray)
			continue;
		KgtBodyColliderManifoldId mid;
		u16 salt;
		if(!kgtBodyColliderParseManifoldHandle(
			body->hManifoldArray, &mid, &salt))
		{
			KLOG(ERROR, "Invalid manifold handle (%li)!", 
			     body->hManifoldArray);
			manifold = nullptr;
			return *this;
		}
		/* update the internal iteration state to point to this body */
		bodyIndex = bid;
		/* conditionally use the first manifold in the body's dynamic manifold 
			array */
		const KgtBodyColliderManifoldId nextManifoldArrayId = (firstBody
			? mid + manifoldArrayId + 1
			: mid);
		if(nextManifoldArrayId < mid + body->manifoldArraySize)
		{
			/* simply set the next manifold to the next element of this 
				body's dynamic manifold array */
			manifold = &kbc->manifoldPool[nextManifoldArrayId];
			/* update the internal iteration state to point to this 
				manifold */
			manifoldArrayId = nextManifoldArrayId;
			return *this;
		}
		firstBody = false;
	}
	/* if we failed to find another active manifold, end of iteration */
	manifold = nullptr;
	return *this;
}
internal KgtBodyColliderManifoldIterator 
	kgtBodyColliderGetManifoldIterator(KgtBodyCollider* bc)
{
	KgtBodyColliderManifoldIterator result;
	result.manifold             = nullptr;
	result.kbc                  = bc;
//	result.visitedManifoldCount = 0;
	result.manifoldArrayId      = 0;
	result.bodyIndex            = bc->memoryReqs.maxBodies;
	for(KgtBodyColliderBodyId bid = 0; bid < bc->memoryReqs.maxBodies; bid++)
	{
		if(!bc->bodySlots[bid].occupied)
			continue;
		if(!bc->bodyPool[bid].hManifoldArray)
			continue;
		kassert(bc->bodyPool[bid].manifoldArraySize > 0);
		kassert(bc->bodyPool[bid].manifoldArrayCapacity >= 
		            bc->bodyPool[bid].manifoldArraySize);
		result.bodyIndex = bid;
		break;
	}
	if(result.bodyIndex >= bc->memoryReqs.maxBodies)
	{
		return result;
	}
	KgtBodyColliderManifoldId mid;
	u16 salt;
	if(!kgtBodyColliderParseManifoldHandle(
			bc->bodyPool[result.bodyIndex].hManifoldArray, &mid, &salt))
		KLOG(ERROR, "Invalid manifold handle (%li)!", 
		     bc->bodyPool[result.bodyIndex].hManifoldArray);
	result.manifold = &bc->manifoldPool[mid];
	return result;
}