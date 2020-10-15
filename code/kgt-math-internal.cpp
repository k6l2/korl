#include "kgt-math-internal.h"
internal void kmath::gjk_initialize(
	GjkState* gjkState, fnSig_gjkSupport* support, void* supportUserData, 
	const v3f32* initialSupportDirection)
{
	if(!initialSupportDirection)
		KLOG(WARNING, "Attempting to solve GJK with no initial support "
		     "direction can lead to SIGNIFICANT performance penalties or "
		     "potentially even no answer due to numerical imprecision!");
	const v3f32& initSupportDir = initialSupportDirection 
		? (initialSupportDirection->isNearlyZero()
			? v3f32{1,0,0}
			: *initialSupportDirection)
		: v3f32{1,0,0};
	gjkState->o_simplex[0]        = support(initSupportDir, supportUserData);
	gjkState->simplexSize         = 1;
	gjkState->searchDirection     = -gjkState->o_simplex[0];
	gjkState->iteration           = 0;
	gjkState->lastIterationResult = GjkIterationResult::INCOMPLETE;
}
/**
 * @return true if we were able to build a complete simplex around the 
 *         origin given the initial points in `o_simplex` after adding 
 *         `newPoint`, false if we require more iterations
 */
internal bool gjk_buildSimplexAroundOrigin(
	v3f32 o_simplex[4], u8* o_simplexSize, const v3f32& newPoint, 
	v3f32* o_searchDirection)
{
	const v3f32 newToOrigin = -newPoint;
	if(*o_simplexSize == 1)
	/* creating a line segment */
	{
		const v3f32 prevToNew = o_simplex[0] - newPoint;
		if(prevToNew.dot(newToOrigin) > 0)
		/* origin is closest to the new line segment */
		{
			*o_searchDirection = 
				prevToNew.cross(newToOrigin).cross(prevToNew);
			if(o_searchDirection->isNearlyZero())
			/* if the origin is essentially colinear with our line segment, 
			 * search in an arbitrary direction perpendicular to the line 
			 * segment */
				*o_searchDirection = prevToNew.cross(prevToNew + v3f32{1,2,3});
			o_simplex[(*o_simplexSize)++] = newPoint;
		}
		else
		/* origin is closest to the new point */
		{
			*o_searchDirection = newToOrigin;
			o_simplex[0] = newPoint;
		}
	}
	else if(*o_simplexSize == 2)
	/* creating a triangle, where the triangle vertices {A,B & C} correspond 
		to {newPoint, o_simplex[1], o_simplex[0]} respectively */
	{
		const v3f32 ab = o_simplex[1] - newPoint;
		const v3f32 ac = o_simplex[0] - newPoint;
		const v3f32 abc = ab.cross(ac);
		const v3f32 abc_x_ac = abc.cross(ac);
		const v3f32 ab_x_abc = ab.cross(abc);
		if(abc_x_ac.dot(newToOrigin) > 0)
			if(ac.dot(newToOrigin) > 0)
			/* origin is closest to the AC line segment */
			{
				*o_searchDirection = ac.cross(newToOrigin).cross(ac);
				o_simplex[1] = newPoint;
			}
			else if(ab.dot(newToOrigin) > 0)
			/* origin is closest to the AB line segment */
			{
				*o_searchDirection = ab.cross(newToOrigin).cross(ab);
				o_simplex[0] = o_simplex[1];
				o_simplex[1] = newPoint;
			}
			else
			/* origin is closest to the new point */
			{
				*o_searchDirection = newToOrigin;
				o_simplex[0] = newPoint;
				*o_simplexSize = 1;
			}
		else if(ab_x_abc.dot(newToOrigin) > 0)
			if(ab.dot(newToOrigin) > 0)
			/* origin is closest to the AB line segment */
			{
				*o_searchDirection = ab.cross(newToOrigin).cross(ab);
				o_simplex[0] = o_simplex[1];
				o_simplex[1] = newPoint;
			}
			else
			/* origin is closest to the new point */
			{
				*o_searchDirection = newToOrigin;
				o_simplex[0] = newPoint;
				*o_simplexSize = 1;
			}
		else /* origin is closest to the ABC triangle */
			if(abc.dot(newToOrigin) > 0)
			/* origin is on the front side (right-handed, remember), so the 
				simplex needs to reverse the triangle normal since the origin is 
				on the opposite side of this simplex face */
			{
				*o_searchDirection = abc;
				/* swap B & C to ensure correct winding in final simplex */
				o_simplex[2] = o_simplex[1];
				o_simplex[1] = o_simplex[0];
				o_simplex[0] = o_simplex[2];
				o_simplex[(*o_simplexSize)++] = newPoint;
			}
			else
			/* origin is on the back side */
			{
				*o_searchDirection = -abc;
				o_simplex[(*o_simplexSize)++] = newPoint;
			}
	}
	else if(*o_simplexSize == 3)
	/* creating a tetrahedron from a triangle, where the triangle vertices 
		{A,B & C} correspond to {o_simplex[2], o_simplex[1], o_simplex[0]} 
		and newPoint is in the opposite direction of the triangle's normal*/
	{
		/* possible cases:
			- origin is outside tetrahedron beyond P in the search direction
				(this case will never occur, since the caller can easily 
				check for this with a simple dot product)
			- origin is in the direction of PBA
			- origin is in the direction of PAC
			- origin is in the direction of PCB
				set the simplex to be the respective triangle with opposing 
				normal & return false
			- origin is within the tetrahedron (none of the above cases)
				return true!!! */
		const v3f32 pa = o_simplex[2] - newPoint;
		const v3f32 pb = o_simplex[1] - newPoint;
		const v3f32 pc = o_simplex[0] - newPoint;
		const v3f32 pba = pb.cross(pa);
		const v3f32 pac = pa.cross(pc);
		const v3f32 pcb = pc.cross(pb);
		if(pba.dot(newToOrigin) > 0)
		{
			*o_searchDirection = pba;
			o_simplex[0] = newPoint;
		}
		else if(pac.dot(newToOrigin) > 0)
		{
			*o_searchDirection = pac;
			o_simplex[1] = newPoint;
		}
		else if(pcb.dot(newToOrigin) > 0)
		{
			*o_searchDirection = pcb;
			o_simplex[2] = newPoint;
		}
		else
		{
			o_simplex[(*o_simplexSize)++] = newPoint;
			return true;
		}
	}
	return false;
}

internal void kmath::gjk_iterate(
	GjkState* gjkState, fnSig_gjkSupport* support, void* supportUserData)
{
	local_persist const u8 MAX_GJK_ITERATIONS = 100;
	if(gjkState->iteration >= MAX_GJK_ITERATIONS)
	{
		KLOG(ERROR, "Maximum GJK iterations reached; the algorithm will most "
		     "likely never converge with the given support function!  This "
		     "should never happen!");
		return;
	}
	if(gjkState->lastIterationResult != GjkIterationResult::INCOMPLETE)
		return;
	const v3f32 newPoint = support(gjkState->searchDirection, supportUserData);
	const f32 newPoint_dot_searchDirection = 
		newPoint.dot(gjkState->searchDirection);
	const u8 simplexSize = gjkState->simplexSize;
	const v3f32 negativeSimplexUnNorm = 
		unNormal(gjkState->o_simplex[0], gjkState->o_simplex[1], 
		         gjkState->o_simplex[2]);
	/* Failure conditions for GJK:
	 * - the new support point wasn't able to cross the origin along the search 
	 *   direction
	 * - if the simplex is currently a triangle, and the search direction equals 
	 *   (-tri.norm) AND the new point is co-planar, this means the origin lies 
	 *   DIRECTLY on the edge of the minkowski difference (or at the very least, 
	 *   we do not have the necessary numerical precision to tell otherwise!).*/
	if(   newPoint_dot_searchDirection < 0 
	   || (   gjkState->simplexSize == 3 
	       //&& isNearlyZero(newPoint_dot_searchDirection)
	       && coplanar(newPoint, gjkState->o_simplex[0], gjkState->o_simplex[1], 
	                   gjkState->o_simplex[2]) 
	       && isNearlyZero(radiansBetween(
	              gjkState->searchDirection, negativeSimplexUnNorm))))
		gjkState->lastIterationResult = GjkIterationResult::FAILURE;
	else if(gjk_buildSimplexAroundOrigin(
			gjkState->o_simplex, &gjkState->simplexSize, newPoint, 
			&gjkState->searchDirection))
		gjkState->lastIterationResult = GjkIterationResult::SUCCESS;
	/* If our previous & new simplex are triangles, and the search direction is 
	 * now > PI/2 difference between iterations, then something must have gone 
	 * wrong due to numerical error and we will most likely never converge! 
	 * Also if the angle between these two vectors is nearly the exact same 
	 * thing, then we will likely not find the simplex */
	else if(simplexSize == 3 && gjkState->simplexSize == 3 
			&& (negativeSimplexUnNorm.dot(gjkState->searchDirection) < 0
				|| isNearlyZero(radiansBetween(
					negativeSimplexUnNorm, gjkState->searchDirection))))
		gjkState->lastIterationResult = GjkIterationResult::FAILURE;
	gjkState->iteration++;
}
internal u8 kmath::gjk_buildSimplexLines(
	GjkState* gjkState, void* o_vertexData, size_t vertexDataBytes, 
	size_t vertexByteStride, size_t vertexPositionOffset)
{
	/* ensure that the data buffer passed to us contains enough memory */
	u8*const o_vertexDataU8 = reinterpret_cast<u8*>(o_vertexData);
	const void*const vertexDataEnd = o_vertexDataU8 + vertexDataBytes;
	u8*const o_vertexPositions = o_vertexDataU8 + vertexPositionOffset;
	/* sizeof(vertexPosition) */
	const size_t requiredVertexBytes = sizeof(v3f32);
	const size_t requiredVertexCount = 12;// maximum emitted vertex count
	kassert(vertexByteStride >= requiredVertexBytes);
	kassert(vertexPositionOffset <= vertexByteStride - sizeof(v3f32));
	kassert(requiredVertexBytes*requiredVertexCount <= vertexDataBytes);
	/* cases: 
		- gjkState->simplexSize == 0?
			do nothing; return 0
		- gjkState->simplexSize == 1?
			draw a 3D crosshair at the simplex vertex; return 6
		- gjkState->simplexSize == 2?
			draw a line; return 2
		- gjkState->simplexSize == 3?
			draw a triangle; return 6
		- gjkState->simplexSize == 4?
			draw a tetrahedron wireframe; return 12 */
	kassert(gjkState->simplexSize <= 4);
	u8 lineVertexPositionsWritten = 0;
	switch(gjkState->simplexSize)
	{
	case 0:
		return 0;
	case 1: {
		local_persist const v3f32 CROSSHAIR_OFFSETS[] = 
			{ {-1,0,0}, {1,0,0}, {0,-1,0}, {0,1,0}, {0,0,-1}, {0,0,1} };
		for(u8 v = 0; v < 6; v++)
			*reinterpret_cast<v3f32*>(o_vertexPositions + v*vertexByteStride) = 
				gjkState->o_simplex[0] + CROSSHAIR_OFFSETS[v];
		return 6;
	} break;
	case 2:
		for(u8 v = 0; v < 2; v++)
			*reinterpret_cast<v3f32*>(o_vertexPositions + 
					v*vertexByteStride) = 
				gjkState->o_simplex[v];
		return 2;
	case 3:{
		local_persist const size_t SIMPLEX_LINE_INDICES[] = { 0,1,1,2,2,0 };
		for(u8 v = 0; v < CARRAY_SIZE(SIMPLEX_LINE_INDICES); v++)
			*reinterpret_cast<v3f32*>(o_vertexPositions + v*vertexByteStride) = 
				gjkState->o_simplex[SIMPLEX_LINE_INDICES[v]];
		return CARRAY_SIZE(SIMPLEX_LINE_INDICES);
	} break;
	case 4:{
		local_persist const size_t SIMPLEX_LINE_INDICES[] = 
			{ 0,1,1,2,2,0, 3,0,3,1,3,2 };
		for(u8 v = 0; v < CARRAY_SIZE(SIMPLEX_LINE_INDICES); v++)
			*reinterpret_cast<v3f32*>(o_vertexPositions + v*vertexByteStride) = 
				gjkState->o_simplex[SIMPLEX_LINE_INDICES[v]];
		return CARRAY_SIZE(SIMPLEX_LINE_INDICES);
	} break;
	}
	return lineVertexPositionsWritten;
}
/**
 * @return The index of the triangle within `epaState->tris` which is closest to 
 *         the origin.
 */
internal void epa_findTriNearestToOrigin(kmath::EpaState* epaState)
{
	epaState->nearestTriToOriginDistance = INFINITY32;
	for(u32 t = 0; t < arrlenu(epaState->tris); t++)
	{
		/* @optimization: cache tri norms & distance to origin, then heapify the 
		 *                array of tris to treat it as a priority queue! */
		const kmath::EpaState::RightHandTri& tri = epaState->tris[t];
		const v3f32 triEdgeA = 
			epaState->vertexPositions[tri.vertexPositionIndices[0]] - 
			epaState->vertexPositions[tri.vertexPositionIndices[1]];
		const v3f32 triEdgeB = 
			epaState->vertexPositions[tri.vertexPositionIndices[0]] - 
			epaState->vertexPositions[tri.vertexPositionIndices[2]];
		const v3f32 triNorm = kmath::normal(triEdgeA.cross(triEdgeB));
		const f32 nearestDistanceToOriginCurrentTri = 
			epaState->vertexPositions[tri.vertexPositionIndices[0]]
			.dot(triNorm);
		if(nearestDistanceToOriginCurrentTri < 
			epaState->nearestTriToOriginDistance)
		{
			epaState->nearestTriToOriginDistance = 
				nearestDistanceToOriginCurrentTri;
			epaState->nearestTriToOriginIndex = t;
			epaState->nearestTriToOriginNormal = triNorm;
		}
	}
}
internal void kmath::epa_initialize(
	EpaState* epaState, const v3f32 simplex[4], KgtAllocatorHandle allocator, 
	f32 resultTolerance)
{
	*epaState = {};
	/* allocate the dynamic memory containers required by EPA, attempting to 
	 * minimize reallocs while at the same time not taking up too much memory */
	epaState->vertexPositions = arrinit(v3f32, allocator);
	arrsetcap(epaState->vertexPositions, 32);
	epaState->tris = arrinit(EpaState::RightHandTri, allocator);
	arrsetcap(epaState->tris, 32);
	/* initialize the EPA polytope to start with the simplex output by GJK (see 
	 * documentation for the `gjk` function for details) */
	for(size_t v = 0; v < 4; v++)
		arrput(epaState->vertexPositions, simplex[v]);
	local_persist const EpaState::RightHandTri SIMPLEX_TRIS[] = 
		{{2,1,0}, {3,2,0}, {3,0,1}, {3,1,2}};
	for(size_t t = 0; t < 4; t++)
	{
		arrput(epaState->tris, SIMPLEX_TRIS[t]);
	}
	/* initialize the EPA by finding the tri closest to the origin */
	epa_findTriNearestToOrigin(epaState);
	/* misc state initialization */
	epaState->resultTolerance = resultTolerance;
	epaState->lastIterationResult = EpaIterationResult::INCOMPLETE;
}
internal u32 epa_hashEdge(u16 vertexIndex0, u16 vertexIndex1)
{
	kassert(vertexIndex0 != vertexIndex1);
	if(vertexIndex0 > vertexIndex1)
	{
		u16 temp = vertexIndex0;
		vertexIndex0 = vertexIndex1;
		vertexIndex1 = temp;
	}
	return (static_cast<u32>(vertexIndex0) << 16) | vertexIndex1;
}
internal void epa_unHashEdge(
	u32 edgeHash, u16* o_vertexIndex0, u16* o_vertexIndex1)
{
	*o_vertexIndex0 = edgeHash >> 16;
	*o_vertexIndex1 = edgeHash & 0xFFFF;
}
/** this is a slow function created to debug crazy memory shenanigans I am 
 * experiencing right now, so this probably shouldn't get called in release
 * deployments 
 * @return false if an invalid triangle is found in the EpaState's tri list
 */
internal bool epa_verifyTriListIntegrity(kmath::EpaState* epaState)
{
	const size_t numTris = arrlenu(epaState->tris);
	for(size_t t = 0; t < numTris; t++)
	{
		const kmath::EpaState::RightHandTri& tri = epaState->tris[t];
		for(i32 v0 = 0; v0 < 3; v0++)
			for(i32 v1 = v0 + 1; v1 < 3; v1++)
				if(tri.vertexPositionIndices[v0] == 
						tri.vertexPositionIndices[v1])
				{
					kassert(false);
					return false;
				}
	}
	return true;
}
internal void kmath::epa_iterate(
	EpaState* epaState, fnSig_gjkSupport* support, void* supportUserData, 
	KgtAllocatorHandle allocator)
{
	if(epaState->lastIterationResult != EpaIterationResult::INCOMPLETE)
		return;
	local_persist const u8 MAX_ITERATIONS = 100;
	if(epaState->iteration >= MAX_ITERATIONS)
	/* If we've reached the maximum # of iterations, silently report a failure 
	 * so the caller can fall back on another minimum-translation-vector 
	 * finding algorithm, such as support sampling.  This should only be likely 
	 * to occur in edge cases, such as two spheres occupying almost the exact 
	 * same position in space */
	{
		epaState->lastIterationResult = EpaIterationResult::FAILURE;
		epa_cleanup(epaState);
		return;
	}
	/* check to see if the current normal is a good enough result */
	const v3f32 supportPoint = 
		support(epaState->nearestTriToOriginNormal, supportUserData);
	const f32 supportPointPolytopeDistance = 
		supportPoint.dot(epaState->nearestTriToOriginNormal);
	const f32 polytopeDistanceToMinkowski = 
		supportPointPolytopeDistance - epaState->nearestTriToOriginDistance;
	if(polytopeDistanceToMinkowski < epaState->resultTolerance)
	/* if the distance between the support point & the polytope along the 
	 * polytope face's normal is within some threshold near zero, then we're 
	 * done! */
	{
		epaState->resultNormal        = -epaState->nearestTriToOriginNormal;
		epaState->resultDistance      = epaState->nearestTriToOriginDistance;
		epaState->lastIterationResult = EpaIterationResult::SUCCESS;
	}
	if(epaState->lastIterationResult != EpaIterationResult::INCOMPLETE)
	/* if we're done, clean up dynamic memory and return */
	{
		epa_cleanup(epaState);
		return;
	}
	/* we're not done yet; expand the polytope using the new support point */
	/* - calculate all the polytope tris whose normals are pointing towards the 
	 *   new support point
	 * - remove all these tris and save a list of all the edges which are not 
	 *   shared between them
	 * - create new tris between the unshared edges and the support point
	 */
	/* @optimization: instead of having to calculate the dot products between 
	 *                tri normals and the vector to the support point for every 
	 *                tri, we can cache the list of 3 adjacent triangles per 
	 *                tri, which would allow us to perform breadth-first-search 
	 *                due to the topology of the polytope!  It should also be 
	 *                possible to completely avoid using KAllocator here by 
	 *                caching `iteration` for each tri, because then we can 
	 *                traverse the tris recursively on the stack!  */
	struct HashedEdge{ u32 key; u8 value; u16 vertexIndices[2]; } 
		*edgeMap = nullptr;
	hminit(edgeMap, allocator);
	defer(hmfree(edgeMap));
	for(size_t t = 0; t < arrlenu(epaState->tris); t++)
	{
		/* figure out if the triangle is facing the support point */
		const kmath::EpaState::RightHandTri& tri = epaState->tris[t];
		const v3f32 triEdgeA = 
			epaState->vertexPositions[tri.vertexPositionIndices[0]] - 
			epaState->vertexPositions[tri.vertexPositionIndices[1]];
		const v3f32 triEdgeB = 
			epaState->vertexPositions[tri.vertexPositionIndices[0]] - 
			epaState->vertexPositions[tri.vertexPositionIndices[2]];
		const v3f32 triUnNorm = triEdgeA.cross(triEdgeB);
		const v3f32 triToSupportPoint = supportPoint - 
			epaState->vertexPositions[tri.vertexPositionIndices[0]];
		if(triUnNorm.dot(triToSupportPoint) < 0)
		{
			continue;
		}
		/* this polytope triangle is pointing towards the support point, and 
		 * therefore must be removed */
		for(u16 v = 0; v < 3; v++)
		/* before removing the triangle, add the edges to the edge map */
		{
			const u16 edgeIndices[2] = 
				{ tri.vertexPositionIndices[v]
				, tri.vertexPositionIndices[(v+1)%3] };
			const u32 edgeKey = epa_hashEdge(edgeIndices[0], edgeIndices[1]);
			HashedEdge hashedEdge = 
				{edgeKey, 1, edgeIndices[0], edgeIndices[1]};
			const ptrdiff_t mappedEdgeIndex = hmgeti(edgeMap, edgeKey);
			if(mappedEdgeIndex >= 0)
			{
				edgeMap[mappedEdgeIndex].value++;
			}
			else
				hmputs(edgeMap, hashedEdge);
		}
		arrdel(epaState->tris, t);
		t--;
	}
	/* at this point, we now know that every element of the edgeMap whose value 
	 * == 1 needs to have a triangle attached to it in the same order as 
	 * `vertexIndices` using `supportPoint` as the third point */
	const u16 supportPointIndex = 
		safeTruncateU16(arrlenu(epaState->vertexPositions));
	for(size_t e = 0; e < hmlenu(edgeMap); e++)
	{
		if(edgeMap[e].value > 1)
			continue;
		kmath::EpaState::RightHandTri tri;
		tri.vertexPositionIndices[0] = edgeMap[e].vertexIndices[0];
		tri.vertexPositionIndices[1] = edgeMap[e].vertexIndices[1];
		tri.vertexPositionIndices[2] = supportPointIndex;
		arrput(epaState->tris, tri);
	}
	arrput(epaState->vertexPositions, supportPoint);
	/* find the next polytope face with the shortest distance to the origin */
	epa_findTriNearestToOrigin(epaState);
	epaState->iteration++;
}
internal void kmath::epa_cleanup(EpaState* epaState)
{
	arrfree(epaState->vertexPositions);
	arrfree(epaState->tris);
}
internal u16 kmath::epa_buildPolytopeTriangles(
	EpaState* epaState, void* o_vertexData, size_t vertexDataBytes, 
	size_t vertexByteStride, size_t vertexPositionOffset, 
	size_t vertexColorOffset)
{
	const size_t requiredVertexCount = 3*(arrlenu(epaState->tris));
	if(!o_vertexData)
	{
		return kmath::safeTruncateU16(requiredVertexCount);
	}
	/* ensure that the data buffer passed to us contains enough memory */
	u8*const o_vertexDataU8 = reinterpret_cast<u8*>(o_vertexData);
	const void*const vertexDataEnd = o_vertexDataU8 + vertexDataBytes;
	u8*const o_vertexPositions = o_vertexDataU8 + vertexPositionOffset;
	u8*const o_vertexColors    = o_vertexDataU8 + vertexColorOffset;
	/* sizeof(vertexPosition) + sizeof(vertexColor) */
	const size_t requiredVertexBytes = sizeof(v3f32) + sizeof(Color4f32);
	kassert(vertexByteStride >= requiredVertexBytes);
	kassert(vertexPositionOffset <= vertexByteStride - sizeof(v3f32));
	kassert(vertexColorOffset    <= vertexByteStride - sizeof(Color4f32));
	kassert(requiredVertexBytes*requiredVertexCount <= vertexDataBytes);
	/* iterate over the polytope triangles and emit the vertex positions */
	for(size_t t = 0; t < arrlenu(epaState->tris); t++)
	{
		const EpaState::RightHandTri& tri = epaState->tris[t];
		/* draw the triangle nearest to the origin green, and the rest white */
		const Color4f32 triColor = (t == epaState->nearestTriToOriginIndex
			? krb::GREEN : krb::WHITE);
		for(size_t e = 0; e < 3; e++)
		{
			*V3F32_STRIDE(o_vertexPositions, vertexByteStride, 3*t + e) = 
				epaState->vertexPositions[tri.vertexPositionIndices[e]];
			*COLOR4F32_STRIDE(o_vertexColors, vertexByteStride, 3*t + e) = 
				triColor;
		}
	}
	return kmath::safeTruncateU16(requiredVertexCount);
}
internal u16 kmath::epa_buildPolytopeEdges(
	EpaState* epaState, void* o_vertexData, size_t vertexDataBytes, 
	size_t vertexByteStride, size_t vertexPositionOffset, 
	KgtAllocatorHandle allocator)
{
	/* first, we need to figure out how many edges there are excluding edges 
	 * which are shared between triangles */
	struct HashedEdge{ u32 key; } *edgeSet = nullptr;
	hminit(edgeSet, allocator);
	defer(hmfree(edgeSet));
	for(size_t t = 0; t < arrlenu(epaState->tris); t++)
	{
		const EpaState::RightHandTri& tri = epaState->tris[t];
		for(size_t v = 0; v < 3; v++)
		{
			HashedEdge hashedEdge = {epa_hashEdge(
				tri.vertexPositionIndices[v      ], 
				tri.vertexPositionIndices[(v+1)%3])};
			const ptrdiff_t edgeIndex = hmgeti(edgeSet, hashedEdge.key);
			if(edgeIndex > -1)
				continue;
			hmputs(edgeSet, hashedEdge);
		}
	}
	/* at this point, we have a set of all unique edges in the polytope! */
	const size_t requiredVertexCount = 2*hmlenu(edgeSet);
	if(!o_vertexData)
		return safeTruncateU16(requiredVertexCount);
	/* ensure that the data buffer passed to us contains enough memory */
	u8*const o_vertexDataU8 = reinterpret_cast<u8*>(o_vertexData);
	const void*const vertexDataEnd = o_vertexDataU8 + vertexDataBytes;
	u8*const o_vertexPositions = o_vertexDataU8 + vertexPositionOffset;
	/* sizeof(vertexPosition) */
	const size_t requiredVertexBytes = sizeof(v3f32);
	kassert(vertexByteStride >= requiredVertexBytes);
	kassert(vertexPositionOffset <= vertexByteStride - sizeof(v3f32));
	kassert(requiredVertexBytes*requiredVertexCount <= vertexDataBytes);
	for(size_t e = 0; e < hmlenu(edgeSet); e++)
	{
		u16 vertexIndices[2];
		epa_unHashEdge(edgeSet[e].key, &vertexIndices[0], &vertexIndices[1]);
		for(size_t v = 0; v < CARRAY_SIZE(vertexIndices); v++)
			*V3F32_STRIDE(o_vertexPositions, vertexByteStride, 2*e + v) = 
				epaState->vertexPositions[vertexIndices[v]];
	}
	return safeTruncateU16(requiredVertexCount);
}
