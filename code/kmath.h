#pragma once
#include "kutil.h"
global_variable const f32 PI32  = 3.14159f;
/* the golden ratio */
global_variable const f32 PHI32 = 1.61803398875f;
#include <limits>
global_variable const f32 INFINITY32 = std::numeric_limits<f32>::infinity();
#include <math.h>
global_variable const f32 NAN32 = nanf("");
#include "kAllocator.h"
struct v2u32
{
	union 
	{
		u32 elements[2];
		struct
		{
			u32 x, y;
		};
	};
	inline v2u32 operator/(u32 discreteScalar) const;
};
struct v2i32
{
	union 
	{
		i32 elements[2];
		struct
		{
			i32 x, y;
		};
	};
public:
	v2i32();
	v2i32(const v2u32& value);
	inline v2i32& operator=(const v2u32& value);
};
struct v2f32
{
	union 
	{
		f32 elements[2];
		struct
		{
			f32 x, y;
		};
	};
	global_variable const v2f32 ZERO;
public:
	inline v2f32 operator-() const;
	inline v2f32 operator*(f32 scalar) const;
	inline v2f32 operator/(f32 scalar) const;
	inline v2f32 operator+(const v2f32& other) const;
	inline v2f32 operator-(const v2f32& other) const;
	inline v2f32& operator*=(const f32 scalar);
	inline v2f32& operator+=(const v2f32& other);
	inline f32 magnitude() const;
	inline f32 magnitudeSquared() const;
	/** @return the magnitude of the vector before normalization */
	inline f32 normalize();
	inline f32 dot(const v2f32& other) const;
	inline v2f32 projectOnto(v2f32 other, bool otherIsNormalized = false) const;
};
struct v3f32
{
	union 
	{
		f32 elements[3];
		struct
		{
			f32 x, y, z;
		};
	};
	global_variable const v3f32 ZERO;
	global_variable const v3f32 X;
	global_variable const v3f32 Y;
	global_variable const v3f32 Z;
public:
	inline v3f32 operator-() const;
	inline v3f32 operator+(const v3f32& other) const;
	inline v3f32 operator-(const v3f32& other) const;
	inline v3f32 operator*(f32 scalar) const;
	inline v3f32 cross(const v3f32& other) const;
	inline v3f32& operator*=(f32 scalar);
	inline v3f32& operator/=(f32 scalar);
	inline v3f32& operator+=(const v3f32& other);
	inline v3f32& operator-=(const v3f32& other);
	inline f32 magnitude() const;
	inline f32 magnitudeSquared() const;
	/** @return the magnitude of the vector before normalization */
	inline f32 normalize();
	inline f32 dot(const v3f32& other) const;
	inline v3f32 projectOnto(v3f32 other, bool otherIsNormalized = false) const;
};
struct v4f32
{
	union 
	{
		f32 elements[4];
		struct
		{
			f32 qw, qx, qy, qz;
		};
		struct
		{
			f32 vx, vy, vz, vw;
		};
		struct
		{
			f32 r, g, b, a;
		};
	};
	global_variable const v4f32 ZERO;
public:
	inline v4f32& operator*=(const f32 scalar);
	inline f32 magnitude() const;
	inline f32 magnitudeSquared() const;
	/** @return the magnitude of the vector before normalization */
	inline f32 normalize();
	inline f32 dot(const v4f32& other) const;
};
/*@TODO: rename this to m4f32 since the x4 can just be implied */
struct m4x4f32
{
	union 
	{
		f32 elements[16];
		struct
		{
			f32 r0c0, r0c1, r0c2, r0c3;
			f32 r1c0, r1c1, r1c2, r1c3;
			f32 r2c0, r2c1, r2c2, r2c3;
			f32 r3c0, r3c1, r3c2, r3c3;
		};
	};
public:
	global_variable const m4x4f32 IDENTITY;
	class_namespace m4x4f32 transpose(const f32* elements);
	class_namespace bool invert(const f32 elements[16], f32 o_elements[16]);
	inline m4x4f32 operator*(const m4x4f32& other) const;
};
struct kQuaternion : public v4f32
{
	class_namespace const kQuaternion IDENTITY;
	// Source: https://en.wikipedia.org/wiki/Quaternion#Hamilton_product
	class_namespace inline kQuaternion hamilton(const kQuaternion& q0, 
	                                            const kQuaternion& q1);
	inline kQuaternion(f32 w = 0.f, f32 x = 0.f, f32 y = 0.f, f32 z = 0.f);
	inline kQuaternion(v3f32 axis, f32 radians, bool axisIsNormalized = false);
	/* Source: 
		https://en.wikipedia.org/wiki/Quaternion#Conjugation,_the_norm,_and_reciprocal */
	inline kQuaternion conjugate() const;
	/* transform is not `const` because it can potentially auto-normalize the 
		quaternion! */
	inline v2f32 transform(const v2f32& v2d, bool quatIsNormalized = false);
	/* transform is not `const` because it can potentially auto-normalize the 
		quaternion! */
	inline v3f32 transform(const v3f32& v3d, bool quatIsNormalized = false);
};
/**
 * a function which is passed a support direction, and returns a position in 
 * space representing the "minkowski difference" for the GJK algorithm below
 */
#define GJK_SUPPORT_FUNCTION(name) \
	v3f32 name(const v3f32& supportDirection, void* userData)
namespace kmath
{
	/* Thanks, Bruce Dawson!  Source: 
		https://randomascii.wordpress.com/2012/02/25/comparing-floating-point-numbers-2012-edition/ */
	internal inline bool isNearlyEqual(f32 fA, f32 fB, f32 epsilon = 1e-5f);
	internal inline bool isNearlyZero(f32 f, f32 epsilon = 1e-5f);
	/** @return the # of bytes which represent `k` kilobytes */
	internal inline u64 kilobytes(u64 k);
	/** @return the # of bytes which represent `m` megabytes */
	internal inline u64 megabytes(u64 m);
	/** @return the # of bytes which represent `g` gigabytes */
	internal inline u64 gigabytes(u64 g);
	/** @return the # of bytes which represent `t` terabytes */
	internal inline u64 terabytes(u64 t);
	internal inline u64 safeTruncateU64(i64 value);
	internal inline u32 safeTruncateU32(u64 value);
	internal inline u32 safeTruncateU32(i32 value);
	internal inline u32 safeTruncateU32(i64 value);
	internal inline i32 safeTruncateI32(u64 value);
	internal inline u16 safeTruncateU16(i32 value);
	internal inline u16 safeTruncateU16(i64 value);
	internal inline u16 safeTruncateU16(u32 value);
	internal inline u16 safeTruncateU16(u64 value);
	internal inline i16 safeTruncateI16(i32 value);
	internal inline u8 safeTruncateU8(u64 value);
	internal inline f32 v2Radians(const v2f32& v);
	internal inline f32 radiansBetween(v2f32 v0, v2f32 v1, 
	                                   bool v0IsNormalized = false, 
	                                   bool v1IsNormalized = false);
	internal inline f32 radiansBetween(v3f32 v0, v3f32 v1, 
	                                   bool v0IsNormalized = false, 
	                                   bool v1IsNormalized = false);
	internal inline v3f32 cross(const v2f32& lhs, const v2f32& rhs);
	internal inline v2f32 normal(v2f32 v);
	internal inline v3f32 normal(v3f32 v);
	internal inline f32 lerp(f32 min, f32 max, f32 ratio);
	internal inline v2f32 rotate(const v2f32& v, f32 radians);
	internal void makeM4f32(const kQuaternion& q, m4x4f32* o_m);
	internal void makeM4f32(
		const kQuaternion& q, const v3f32& translation, m4x4f32* o_m);
	/**
	 * Find the roots of a given quadratic formula which satisfies the form 
	 * `f(x) = ax^2 + bx + c`.  The roots are values of `x` which will yield 
	 * `f(x) = 0`.  Complex# solutions are ignored.
	 * @param o_roots The locations in memory where the roots will be stored, if 
	 *                they exist.  The roots will be stored in ascending order.  
	 *                Only the first N values will be written to, where N is the 
	 *                return value of this function.  In the RARE case that the 
	 *                equation is a line along the x-axis, o_roots[0] is set to 
	 *                INFINITY32 & a value of 0 is returned.
	 * @return The number of UNIQUE real# roots found for the given equation.  
	 *         This value will always be in the range [0,2].
	 */
	internal u8 solveQuadratic(f32 a, f32 b, f32 c, f32 o_roots[2]);
	/**
	 * @return (1) NAN32 if the ray does not collide with the plane.  
	 *         (2) INFINITY32 if the ray is co-planar with the plane.  
	 *         (3) Otherwise, a scalar representing how far from `rayOrigin` in 
	 *             the direction of `rayNormal` the intersection is.
	 *         In case (3), the return value will ALWAYS be positive.
	 */
	internal f32 collideRayPlane(
		const v3f32& rayOrigin, const v3f32& rayNormal, 
		const v3f32& planeNormal, f32 planeDistanceFromOrigin, 
		bool cullPlaneBackFace);
	/**
	 * @return (1) NAN32 if the ray does not collide with the sphere
	 *         (2) Otherwise, a scalar representing how far from `rayOrigin` in 
	 *             the direction of `rayNormal` the intersection is.
	 *         If the ray origin is contained within the sphere, 0 is returned.
	 */
	internal f32 collideRaySphere(
		const v3f32& rayOrigin, const v3f32& rayNormal, 
		const v3f32& sphereOrigin, f32 sphereRadius);
	/**
	 * @return (1) NAN32 if the ray does not collide with the box
	 *         (2) Otherwise, a scalar representing how far from `rayOrigin` in 
	 *             the direction of `rayNormal` the intersection is.
	 *         If the ray origin is contained within the box, 0 is returned.
	 */
	internal f32 collideRayBox(
		const v3f32& rayOrigin, const v3f32& rayNormal, 
		const v3f32& boxWorldPosition, const kQuaternion& boxOrientation, 
		const v3f32& boxLengths);
	/**
	 * Generate mesh data for a box centered on the origin with `lengths` widths 
	 * for each respective dimension of the `lengths` parameter.
	 * @param lengths The total length in each dimension of the box.  The final 
	 *                vertex positions of the box will not exceed 
	 *                `lengths.? / 2`, where `?` is the element in each 
	 *                respective dimension.
	 * @param o_buffer Pointer to the array where the vertex data will be 
	 *                 stored.  Caller must supply an array of >= 36 elements in 
	 *                 order to contain a complete 3D box!  Output vertices are 
	 *                 a list of right-handed triangles.
	 */
	internal void generateMeshBox(
		v3f32 lengths, void* o_vertexData, size_t vertexDataBytes, 
		size_t vertexByteStride, size_t vertexPositionOffset, 
		size_t vertexTextureNormalOffset);
	internal size_t generateMeshCircleSphereVertexCount(
		u32 latitudeSegments, u32 longitudeSegments);
	/**
	 * Generate mesh data for a sphere made of (semi)circles along latitudes and 
	 * longitudes with the specified respective resolutions.
	 * @param latitudeSegments The # of latitude circles in the sphere equals 
	 *                         `latitudeSegments - 1`, as the sphere's "top" and 
	 *                         "bottom" vertices will define `latitudeSegments` 
	 *                         strips of solid geometry.  
	 *                         This value MUST be >= 2.
	 * @param longitudeSegments The # of longitude semicircles in the sphere.  
	 *                          This value MUST be >= 3.
	 * @param o_buffer Pointer to the array where the vertex data will be 
	 *                 stored.  Caller must supply an array of >= 
	 *                 `generateMeshCircleSphereVertexCount(...)` elements!  
	 *                 Output vertices are a list of right-handed triangles.  
	 *                 Vertex textureNormals are generated using a cylindrical 
	 *                 projection.
	 */
	internal void generateMeshCircleSphere(
		f32 radius, u32 latitudeSegments, u32 longitudeSegments, 
		void* o_vertexData, size_t vertexDataBytes, size_t vertexByteStride, 
		size_t vertexPositionOffset, size_t vertexTextureNormalOffset);
	internal void generateUniformSpherePoints(
		u32 pointCount, void* o_vertexData, size_t vertexDataBytes, 
		size_t vertexByteStride, size_t vertexPositionOffset);
	/**
	 * @return the local position on the shape with the highest dot product with 
	 *         the supportDirection
	 */
	internal inline v3f32 supportSphere(
		f32 radius, v3f32 supportDirection, 
		bool supportDirectionIsNormalized = false);
	/**
	 * @return the local position on the shape with the highest dot product with 
	 *         the supportDirection
	 */
	internal inline v3f32 supportBox(
		v3f32 lengths, kQuaternion orientation, 
		v3f32 supportDirection, bool supportDirectionIsNormalized = false);
	typedef GJK_SUPPORT_FUNCTION(fnSig_gjkSupport);
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
	/**
	 * Gilbert–Johnson–Keerthi algorithm implementation.
	 * @param o_simplex if the return value is true, this will contain the 
	 *                  vertices of a tetrahedron which contains the origin 
	 *                  whose vertex indices for each triangle are:
	 *                  {{2,1,0}, {3,2,0}, {3,0,1}, {3,1,2}}
	 * @return true if there exists a tetrahedron within the support function's 
	 *         bounds which contains the origin
	 */
	internal bool gjk(
		fnSig_gjkSupport* support, void* supportUserData, v3f32 o_simplex[4], 
		const v3f32* initialSupportDirection = nullptr);
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
	/**
	 * Expanding Polytope algorithm implementation.
	 * @param simplex The output of the `gjk` function when it returns true.  
	 *                This set of tetrahedron vertices MUST enclose the origin 
	 *                of the minkowski space defined by the support function in 
	 *                order for this algorithm to work!
	 * @param allocator It's probably a good idea to use a frame allocator here.
	 */
	internal v3f32 epa(
		fnSig_gjkSupport* support, void* supportUserData, 
		const v3f32 simplex[4], KAllocatorHandle allocator);
}
internal inline v2f32 operator*(f32 lhs, const v2f32& rhs);
internal inline v3f32 operator*(f32 lhs, const v3f32& rhs);
internal inline v3f32 operator/(const v3f32& lhs, f32 rhs);
internal v4f32 operator*(const m4x4f32& lhs, const v4f32& rhs);
internal inline kQuaternion operator*(const kQuaternion& lhs, 
                                      const kQuaternion& rhs);
