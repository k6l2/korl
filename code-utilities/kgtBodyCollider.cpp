#include "kgtBodyCollider.h"
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
	KGT_BODY_COLLIDER_CIRCLE_SPHERE_LATITUDE_SEGMENTS  = 16;
global_variable const u32 
	KGT_BODY_COLLIDER_CIRCLE_SPHERE_LONGITUDE_SEGMENTS = 16;
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
			const v4f32 color = {1,1,1,1};
			///@TODO
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
	/* iterate over each manifold:
		iterate over each contact point:
			- draw a line starting at the contact point ending at the MTV 
				relative to that point
			- color the line vertices cyan-magenta so we can tell where the line 
				begins and in what direction it is going */
	///@TODO
	return vertexCount;
}