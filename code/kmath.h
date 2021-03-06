#pragma once
#include "kutil.h"
global_variable const f32 PI32  = 3.14159f;
/* the golden ratio */
global_variable const f32 PHI32 = 1.61803398875f;
#include <limits>
global_variable const f32 INFINITY32 = std::numeric_limits<f32>::infinity();
#include <math.h>
global_variable const f32 NAN32 = nanf("");
#include "kgtAllocator.h"
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
public:
	inline v2u32 operator*(u32 discreteScalar) const;
	inline v2u32 operator/(u32 discreteScalar) const;
	inline v2u32 operator*(const v2u32& other) const;
	inline v2u32 operator+(const v2u32& other) const;
	inline v2u32 operator-(const v2u32& other) const;
	inline bool operator==(const v2u32& other) const;
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
	inline v2f32 operator*(const v2f32& other) const;
	inline v2f32 operator/(const v2f32& other) const;
	inline v2f32 operator+(const v2f32& other) const;
	inline v2f32 operator-(const v2f32& other) const;
	inline v2f32& operator*=(const f32 scalar);
	inline v2f32& operator/=(const f32 scalar);
	inline v2f32& operator+=(const v2f32& other);
	inline bool isNearlyZero() const;
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
	inline bool isNearlyZero() const;
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
			f32 x, y, z, w;
		};
		struct
		{
			f32 r, g, b, a;
		};
	};
	global_variable const v4f32 ZERO;
public:
	/** Threshold equality; NOT an exact bit-for-bit equality! */
	inline bool operator==(const v4f32& other) const;
	inline v4f32& operator*=(const f32 scalar);
	inline f32 magnitude() const;
	inline f32 magnitudeSquared() const;
	/** @return the magnitude of the vector before normalization */
	inline f32 normalize();
	inline f32 dot(const v4f32& other) const;
	/** @param ratio 
	 * 0: the result is entirely `this`
	 * 1: the result is entirely `other` */
	inline v4f32 lerp(const v4f32& other, f32 ratio) const;
};
struct m4f32
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
	global_variable const m4f32 IDENTITY;
	class_namespace m4f32 transpose(const f32* elements);
	class_namespace bool invert(const f32 elements[16], f32 o_elements[16]);
	inline m4f32 operator*(const m4f32& other) const;
};
struct q32 : public v4f32
{
	class_namespace const q32 IDENTITY;
	// Source: https://en.wikipedia.org/wiki/Quaternion#Hamilton_product
	class_namespace inline q32 hamilton(const q32& q0, const q32& q1);
	inline q32(f32 w = 0.f, f32 x = 0.f, f32 y = 0.f, f32 z = 0.f);
	inline q32(v3f32 axis, f32 radians, bool axisIsNormalized = false);
	/* Source: 
		https://en.wikipedia.org/wiki/Quaternion#Conjugation,_the_norm,_and_reciprocal */
	inline q32 conjugate() const;
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
 * @TODO: rename this since it is no longer used by just GJK
 */
#define GJK_SUPPORT_FUNCTION(name) \
	v3f32 name(const v3f32& supportDirection, void* userData)
typedef GJK_SUPPORT_FUNCTION(fnSig_gjkSupport);
namespace kmath
{
	/* Thanks, Bruce Dawson!  Source: 
		https://randomascii.wordpress.com/2012/02/25/comparing-floating-point-numbers-2012-edition/ */
	internal inline bool isNearlyEqual(f32 fA, f32 fB, f32 epsilon = 1e-5f);
	internal inline bool isNearlyEqual(f64 fA, f64 fB, f64 epsilon = 1e-5f);
	internal inline bool isNearlyZero(f32 f, f32 epsilon = 1e-5f);
	internal inline f32 abs(f32 f);
	internal inline i32 abs(i32 x);
	internal inline f32    min(f32    a, f32    b);
	internal inline size_t min(size_t a, size_t b);
	/** @return the # of bytes which represent `k` kilobytes */
	internal inline u64 kilobytes(u64 k);
	/** @return the # of bytes which represent `m` megabytes */
	internal inline u64 megabytes(u64 m);
	/** @return the # of bytes which represent `g` gigabytes */
	internal inline u64 gigabytes(u64 g);
	/** @return the # of bytes which represent `t` terabytes */
	internal inline u64 terabytes(u64 t);
	internal inline f32 safeTruncateF32(f64 value);
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
	internal inline u8 safeTruncateU8(f32 value);
	internal inline u8 safeTruncateU8(i32 value);
	internal inline u8 safeTruncateU8(u32 value);
	internal inline u8 safeTruncateU8(u64 value);
	internal inline i8 safeTruncateI8(i32 value);
	internal inline f32 v2Radians(const v2f32& v);
	internal inline f32 radiansBetween(
		v2f32 v0, v2f32 v1, 
		bool v0IsNormalized = false, bool v1IsNormalized = false);
	internal inline f32 radiansBetween(
		v3f32 v0, v3f32 v1, 
		bool v0IsNormalized = false, bool v1IsNormalized = false);
	internal inline v3f32 cross(const v2f32& lhs, const v2f32& rhs);
	internal inline v2f32 normal(v2f32 v);
	internal inline v3f32 normal(v3f32 v);
	/** @return 
	 * The right-handed normal of the triangle defined by the 3 parameters. */
	internal inline v3f32 normal(v3f32 p0, v3f32 p1, v3f32 p2);
	/** @return 
	 * The right-handed un-normalized normal of the triangle defined by the 3 
	 * parameters. */
	internal inline v3f32 unNormal(
		const v3f32& p0, const v3f32& p1, const v3f32& p2);
	internal inline f32 lerp(f32 min, f32 max, f32 ratio);
	internal inline v2f32 rotate(const v2f32& v, f32 radians);
	internal bool coplanar(
		const v3f32& p0, const v3f32& p1, const v3f32& p2, const v3f32& p3);
	internal void makeM4f32(const q32& q, m4f32* o_m);
	internal void 
		makeM4f32(const q32& q, const v3f32& translation, m4f32* o_m);
	internal void 
		makeM4f32(
			const q32& q, const v3f32& translation, const v3f32& scale, 
			m4f32* o_m);
	/**
	 * @return A sine curve that lies in the range [0,1]
	 */
	internal inline f32 sine_0_1(f32 radians);
	internal inline f32 clamp(f32 x, f32 min, f32 max);
	internal inline i32 clamp(i32 x, i32 min, i32 max);
	internal inline f32 max(f32 a, f32 b);
	internal inline u32 max(u32 a, u32 b);
	internal inline i32 roundToI32(f32 x);
	/**
	 * Find the roots of a given quadratic formula which satisfies the form 
	 * `f(x) = ax^2 + bx + c`.  The roots are values of `x` which will yield 
	 * `f(x) = 0`.  Complex# solutions are ignored.
	 * @param o_roots 
	 * The locations in memory where the roots will be stored, if they exist.  
	 * The roots will be stored in ascending order.  Only the first N values 
	 * will be written to, where N is the return value of this function.  In the 
	 * RARE case that the equation is a line along the x-axis, o_roots[0] is set 
	 * to INFINITY32 & a value of 0 is returned.
	 * @return 
	 * The number of UNIQUE real# roots found for the given equation.  This 
	 * value will always be in the range [0,2].
	 */
	internal u8 solveQuadratic(f32 a, f32 b, f32 c, f32 o_roots[2]);
	/**
	 * @return 
	 * (1) NAN32 if the ray does not collide with the plane.  
	 * (2) INFINITY32 if the ray is co-planar with the plane.  
	 * (3) Otherwise, a scalar representing how far from `rayOrigin` in the 
	 *     direction of `rayNormal` the intersection is.
	 * In case (3), the return value will ALWAYS be positive.
	 */
	internal f32 collideRayPlane(
		const v3f32& rayOrigin, const v3f32& rayNormal, 
		const v3f32& planeNormal, f32 planeDistanceFromOrigin, 
		bool cullPlaneBackFace);
	/**
	 * @return 
	 * (1) NAN32 if the ray does not collide with the sphere
	 * (2) Otherwise, a scalar representing how far from `rayOrigin` in 
	 *     the direction of `rayNormal` the intersection is.
	 * If the ray origin is contained within the sphere, 0 is returned. 
	 */
	internal f32 collideRaySphere(
		const v3f32& rayOrigin, const v3f32& rayNormal, 
		const v3f32& sphereOrigin, f32 sphereRadius);
	/**
	 * @return 
	 * (1) NAN32 if the ray does not collide with the box
	 * (2) Otherwise, a scalar representing how far from `rayOrigin` in 
	 *     the direction of `rayNormal` the intersection is.
	 * If the ray origin is contained within the box, 0 is returned.
	 */
	internal f32 collideRayBox(
		const v3f32& rayOrigin, const v3f32& rayNormal, 
		const v3f32& boxWorldPosition, const q32& boxOrientation, 
		const v3f32& boxLengths);
	/**
	 * Generate mesh data for a box centered on the origin with `lengths` widths 
	 * for each respective dimension of the `lengths` parameter.
	 * @param lengths 
	 * The total length in each dimension of the box.  The final vertex 
	 * positions of the box will not exceed `lengths.? / 2`, where `?` is the 
	 * element in each respective dimension.
	 * @param o_buffer 
	 * Pointer to the array where the vertex data will be stored.  Caller must 
	 * supply an array of >= 36 elements in order to contain a complete 3D box!  
	 * Output vertices are a list of right-handed triangles.
	 */
	internal void generateMeshBox(
		v3f32 lengths, void* o_vertexData, size_t vertexDataBytes, 
		size_t vertexByteStride, size_t vertexPositionOffset, 
		size_t vertexTextureNormalOffset);
	internal u32 generateMeshCircleSphereVertexCount(
		u32 latitudeSegments, u32 longitudeSegments);
	/**
	 * Generate mesh data for a sphere made of (semi)circles along latitudes and 
	 * longitudes with the specified respective resolutions.
	 * @param latitudeSegments 
	 * The # of latitude circles in the sphere equals `latitudeSegments - 1`, as 
	 * the sphere's "top" and "bottom" vertices will define `latitudeSegments` 
	 * strips of solid geometry.  This value MUST be >= 2.
	 * @param longitudeSegments 
	 * The # of longitude semicircles in the sphere.  This value MUST be >= 3.
	 * @param o_buffer 
	 * Pointer to the array where the vertex data will be stored.  Caller must 
	 * supply an array of >= `generateMeshCircleSphereVertexCount(...)` 
	 * elements!  Output vertices are a list of right-handed triangles.  Vertex 
	 * textureNormals are generated using a cylindrical projection.
	 */
	internal void generateMeshCircleSphere(
		f32 radius, u32 latitudeSegments, u32 longitudeSegments, 
		void* o_vertexData, size_t vertexDataBytes, size_t vertexByteStride, 
		size_t vertexPositionOffset, size_t vertexTextureNormalOffset);
	internal size_t 
		generateMeshCircleSphereWireframeVertexCount(
			u32 latitudeSegments, u32 longitudeSegments);
	internal void 
		generateMeshCircleSphereWireframe(
			f32 radius, u32 latitudeSegments, u32 longitudeSegments, 
			void* o_vertexData, size_t vertexDataBytes, size_t vertexByteStride, 
			size_t vertexPositionOffset);
	/** 
	 * create a MOSTLY even distribution of points around a unit sphere using 
	 * the golden ratio.  
	 * Source: https://stackoverflow.com/a/44164075/4526664 
	 */
	internal void generateUniformSpherePoints(
		u32 pointCount, void* o_vertexData, size_t vertexDataBytes, 
		size_t vertexByteStride, size_t vertexPositionOffset);
	/**
	 * @return 
	 * the local position on the shape with the highest dot product with the 
	 * supportDirection
	 */
	internal inline v3f32 supportSphere(
		f32 radius, v3f32 supportDirection, 
		bool supportDirectionIsNormalized = false);
	/**
	 * @return 
	 * the local position on the shape with the highest dot product with the 
	 * supportDirection
	 */
	internal inline v3f32 supportBox(
		v3f32 lengths, q32 orient, v3f32 supportDirection, 
		bool supportDirectionIsNormalized = false);
	/**
	 * Gilbert–Johnson–Keerthi algorithm implementation.
	 * @param o_simplex 
	 * if the return value is true, this will contain the vertices of a 
	 * tetrahedron which contains the origin whose vertex indices for each 
	 * triangle are: {{2,1,0}, {3,2,0}, {3,0,1}, {3,1,2}}
	 * @return 
	 * true if there exists a tetrahedron within the support function's bounds 
	 * which contains the origin
	 */
	internal bool gjk(
		fnSig_gjkSupport* support, void* supportUserData, v3f32 o_simplex[4], 
		const v3f32* initialSupportDirection = nullptr);
	/**
	 * Expanding Polytope algorithm implementation.
	 * @param simplex 
	 * The output of the `gjk` function when it returns true.  This set of 
	 * tetrahedron vertices MUST enclose the origin of the minkowski space 
	 * defined by the support function in order for this algorithm to work!
	 * @param allocator 
	 * It's probably a good idea to use a frame allocator here.
	 * @return 
	 * true if the algorithm was able to converge on a minimum translation 
	 * vector, false otherwise.  If an MTV is found, it will be stored in the 
	 * appropriate `o_*` parameters.
	 */
	internal bool epa(
		v3f32* o_minimumTranslationNormal, f32* o_minimumTranslationDistance, 
		fnSig_gjkSupport* support, void* supportUserData, 
		const v3f32 simplex[4], KgtAllocatorHandle allocator, 
		f32 resultTolerance = 0.0001f);
}
internal inline v2f32 operator*(f32 lhs, const v2f32& rhs);
internal inline v3f32 operator*(f32 lhs, const v3f32& rhs);
internal inline v3f32 operator/(const v3f32& lhs, f32 rhs);
internal inline v2f32 operator/(const v2f32& lhs, const v2u32& rhs);
internal inline v2f32 operator/(const v2u32& lhs, const v2f32& rhs);
internal inline v2f32 operator*(const v2u32& lhs, const v2f32& rhs);
internal inline v2f32 operator*(const v2f32& lhs, const v2u32& rhs);
internal v4f32 operator*(const m4f32& lhs, const v4f32& rhs);
internal inline q32 operator*(const q32& lhs, const q32& rhs);
internal inline v2f32 operator-(const v2f32& lhs, const v2u32& rhs);
internal inline v2f32 operator-(const v2u32& lhs, const v2f32& rhs);
