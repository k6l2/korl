#pragma once
namespace kmath
{
	enum class GjkIterationResult : u8
		{ FAILURE, INCOMPLETE, SUCCESS };
	struct GjkState
	{
		v3f32 o_simplex[4];
		v3f32 searchDirection;
		u8 simplexSize;
		u8 iteration;
		GjkIterationResult lastIterationResult;
	};
	internal void gjk_initialize(
		GjkState* gjkState, fnSig_gjkSupport* support, void* supportUserData, 
		const v3f32* initialSupportDirection = nullptr);
	internal void gjk_iterate(
		GjkState* gjkState, fnSig_gjkSupport* support, void* supportUserData);
	/**
	 * @return The # of vertex positions written, corresponding to #/2 lines.  
	 *         This value should always be in the range [0,12], and it should 
	 *         always be even because each line requires 2 vertices.
	 */
	internal u8 gjk_buildSimplexLines(
		GjkState* gjkState, void* o_vertexData, size_t vertexDataBytes, 
		size_t vertexByteStride, size_t vertexPositionOffset);
	enum class EpaIterationResult : u8
		{ FAILURE, INCOMPLETE, SUCCESS };
	struct EpaState
	{
		/** stb_ds array */
		v3f32* vertexPositions;
		/** the normal of a triangle is defined as (v1-v0).cross(v2-v0) */
		struct RightHandTri
		{
			/** each u16 represents an index of the `vertices` array */
			u16 vertexPositionIndices[3];
		} *tris;/** stb_ds array of triangles */
		f32 resultTolerance;
		u32   nearestTriToOriginIndex;
		f32   nearestTriToOriginDistance;
		v3f32 nearestTriToOriginNormal;
		v3f32 resultNormal;
		f32   resultDistance;
		u8 iteration;
		EpaIterationResult lastIterationResult;
	};
	/** remember to call `epa_cleanup` after calling this if `allocator` uses 
	 * persistent memory storage and the EPA is aborted early!!!  If 
	 * `epa_iterate` is called until the algorithm returns 
	 * `EpaIterationResult::SUCCESS` then there is no need to manually cleanup 
	 * resources. */
	internal void epa_initialize(
		EpaState* epaState, const v3f32 simplex[4], KAllocatorHandle allocator, 
		f32 resultTolerance = 0.0001f);
	internal void epa_iterate(
		EpaState* epaState, fnSig_gjkSupport* support, void* supportUserData, 
		KAllocatorHandle allocator);
	internal void epa_cleanup(EpaState* epaState);
	/**
	 * @return The required number of triangle vertices written to 
	 *         `o_vertexData`.  If `o_vertexData` == nullptr, no actual work is 
	 *         performed and the required vertex count is returned immediately.
	 */
	internal u16 epa_buildPolytopeTriangles(
		EpaState* epaState, void* o_vertexData, size_t vertexDataBytes, 
		size_t vertexByteStride, size_t vertexPositionOffset, 
		size_t vertexColorOffset);
	/**
	 * @return The required number of line vertices written to 
	 *         `o_vertexData`.  If `o_vertexData` == nullptr, the required 
	 *         vertex count is calculated and returned immediately (this process
	 *         requires dynamic memory allocation).
	 */
	internal u16 epa_buildPolytopeEdges(
		EpaState* epaState, void* o_vertexData, size_t vertexDataBytes, 
		size_t vertexByteStride, size_t vertexPositionOffset, 
		KAllocatorHandle allocator);
}
