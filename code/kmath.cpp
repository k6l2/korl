#include "kmath.h"
#include <limits>
/* no idea where these stupid macros are coming from, but I don't care since 
	they are probably worthless anyway */
#if defined(max)
	#undef max
#endif
#if defined(min)
	#undef min
#endif
const v2f32 v2f32::ZERO = {0,0};
const v3f32 v3f32::ZERO = {0,0,0};
const v3f32 v3f32::X = {1,0,0};
const v3f32 v3f32::Y = {0,1,0};
const v3f32 v3f32::Z = {0,0,1};
const v4f32 v4f32::ZERO = {0,0,0,0};
const kQuaternion kQuaternion::IDENTITY = {1,0,0,0};
const m4x4f32 m4x4f32::IDENTITY = {1,0,0,0,
                                   0,1,0,0,
                                   0,0,1,0,
                                   0,0,0,1};
inline v2u32 v2u32::operator/(u32 discreteScalar) const
{
	kassert(discreteScalar != 0);
	return {x / discreteScalar, y / discreteScalar};
}
v2i32::v2i32()
{
}
v2i32::v2i32(const v2u32& value)
{
	x = kmath::safeTruncateI32(value.x);
	y = kmath::safeTruncateI32(value.y);
}
inline v2i32& v2i32::operator=(const v2u32& value)
{
	x = kmath::safeTruncateI32(value.x);
	y = kmath::safeTruncateI32(value.y);
	return *this;
}
inline v2f32 v2f32::operator-() const
{
	return {-x,-y};
}
inline v2f32 v2f32::operator*(f32 scalar) const
{
	return {scalar*x, scalar*y};
}
inline v2f32 v2f32::operator/(f32 scalar) const
{
	return {x/scalar, y/scalar};
}
inline v2f32 v2f32::operator+(const v2f32& other) const
{
	return {x + other.x, y + other.y};
}
inline v2f32 v2f32::operator-(const v2f32& other) const
{
	return {x - other.x, y - other.y};
}
inline v2f32& v2f32::operator*=(const f32 scalar)
{
	x *= scalar;
	y *= scalar;
	return *this;
}
inline v2f32& v2f32::operator+=(const v2f32& other)
{
	x += other.x;
	y += other.y;
	return *this;
}
inline f32 v2f32::magnitude() const
{
	return sqrtf(powf(x,2) + powf(y,2));
}
inline f32 v2f32::magnitudeSquared() const
{
	return powf(x,2) + powf(y,2);
}
inline f32 v2f32::dot(const v2f32& other) const
{
	return x*other.x + y*other.y;
}
inline f32 v2f32::normalize()
{
	const f32 mag = magnitude();
	if(kmath::isNearlyZero(mag))
	{
		return x = y = 0;
	}
	x /= mag;
	y /= mag;
	return mag;
}
inline v2f32 v2f32::projectOnto(v2f32 other, bool otherIsNormalized) const
{
	if(!otherIsNormalized)
	{
		other.normalize();
	}
	const f32 scalarProjection = dot(other);
	return scalarProjection*other;
}
internal inline v2f32 operator*(f32 lhs, const v2f32& rhs)
{
	return {lhs*rhs.x, lhs*rhs.y};
}
internal inline v3f32 operator*(f32 lhs, const v3f32& rhs)
{
	return {lhs*rhs.x, lhs*rhs.y, lhs*rhs.z};
}
inline v3f32 v3f32::operator+(const v3f32& other) const
{
	return {x + other.x, y + other.y, z + other.z};
}
inline v3f32 v3f32::operator-(const v3f32& other) const
{
	return {x - other.x, y - other.y, z - other.z};
}
inline v3f32 v3f32::operator*(f32 scalar) const
{
	return {scalar*x, scalar*y, scalar*z};
}
inline v3f32 v3f32::cross(const v3f32& other) const
{
	return { y*other.z - z*other.y
	       , z*other.x - x*other.z
	       , x*other.y - y*other.x };
}
inline v3f32& v3f32::operator/=(f32 scalar)
{
	kassert(!kmath::isNearlyZero(scalar));
	x /= scalar;
	y /= scalar;
	z /= scalar;
	return *this;
}
inline v3f32& v3f32::operator+=(const v3f32& other)
{
	x += other.x;
	y += other.y;
	z += other.z;
	return *this;
}
inline v3f32& v3f32::operator-=(const v3f32& other)
{
	x -= other.x;
	y -= other.y;
	z -= other.z;
	return *this;
}
inline f32 v3f32::magnitude() const
{
	return sqrtf(powf(x,2) + powf(y,2) + powf(z,2));
}
inline f32 v3f32::magnitudeSquared() const
{
	return powf(x,2) + powf(y,2) + powf(z,2);
}
inline f32 v3f32::normalize()
{
	const f32 mag = magnitude();
	if(kmath::isNearlyZero(mag))
	{
		return x = y = z = 0;
	}
	x /= mag;
	y /= mag;
	z /= mag;
	return mag;
}
inline f32 v3f32::dot(const v3f32& other) const
{
	return x*other.x + y*other.y + z*other.z;
}
inline v3f32 v3f32::projectOnto(v3f32 other, bool otherIsNormalized) const
{
	if(!otherIsNormalized)
	{
		other.normalize();
	}
	const f32 scalarProjection = dot(other);
	return scalarProjection*other;
}
inline v4f32& v4f32::operator*=(const f32 scalar)
{
	vx *= scalar;
	vy *= scalar;
	vz *= scalar;
	vw *= scalar;
	return *this;
}
inline f32 v4f32::magnitude() const
{
	return sqrtf(powf(vw,2) + powf(vx,2) + powf(vy,2) + powf(vz,2));
}
inline f32 v4f32::magnitudeSquared() const
{
	return powf(vw,2) + powf(vx,2) + powf(vy,2) + powf(vz,2);
}
inline f32 v4f32::normalize()
{
	const f32 mag = magnitude();
	if(kmath::isNearlyZero(mag))
	{
		return vw = vx = vy = vz = 0;
	}
	vw /= mag;
	vx /= mag;
	vy /= mag;
	vz /= mag;
	return mag;
}
inline f32 v4f32::dot(const v4f32& other) const
{
	return vw*other.vw + vx*other.vx + vy*other.vy + vz*other.vz;
}
m4x4f32 m4x4f32::transpose(const f32* elements)
{
	m4x4f32 result = {};
	for(u8 r = 0; r < 4; r++)
	{
		for(u8 c = 0; c < 4; c++)
		{
			result.elements[4*r + c] = elements[4*c + r];
		}
	}
	return result;
}
bool m4x4f32::invert(const f32 elements[16], f32 o_elements[16])
{
	/* implementation derived from MESA's GLU lib: 
		https://stackoverflow.com/a/1148405/4526664 
		@TODO: investigate MESA's license, or maybe just roll my own code here? 
		*/
	f32 inv[16];
	inv[0] = elements[5]  * elements[10] * elements[15] - 
	         elements[5]  * elements[11] * elements[14] - 
	         elements[9]  * elements[6]  * elements[15] + 
	         elements[9]  * elements[7]  * elements[14] +
	         elements[13] * elements[6]  * elements[11] - 
	         elements[13] * elements[7]  * elements[10];
	inv[4] = -elements[4]  * elements[10] * elements[15] + 
	          elements[4]  * elements[11] * elements[14] + 
	          elements[8]  * elements[6]  * elements[15] - 
	          elements[8]  * elements[7]  * elements[14] - 
	          elements[12] * elements[6]  * elements[11] + 
	          elements[12] * elements[7]  * elements[10];
	inv[8] = elements[4]  * elements[9] * elements[15] - 
	         elements[4]  * elements[11] * elements[13] - 
	         elements[8]  * elements[5] * elements[15] + 
	         elements[8]  * elements[7] * elements[13] + 
	         elements[12] * elements[5] * elements[11] - 
	         elements[12] * elements[7] * elements[9];
	inv[12] = -elements[4]  * elements[9] * elements[14] + 
	           elements[4]  * elements[10] * elements[13] +
	           elements[8]  * elements[5] * elements[14] - 
	           elements[8]  * elements[6] * elements[13] - 
	           elements[12] * elements[5] * elements[10] + 
	           elements[12] * elements[6] * elements[9];
	inv[1] = -elements[1]  * elements[10] * elements[15] + 
	          elements[1]  * elements[11] * elements[14] + 
	          elements[9]  * elements[2] * elements[15] - 
	          elements[9]  * elements[3] * elements[14] - 
	          elements[13] * elements[2] * elements[11] + 
	          elements[13] * elements[3] * elements[10];
	inv[5] = elements[0]  * elements[10] * elements[15] - 
	         elements[0]  * elements[11] * elements[14] - 
	         elements[8]  * elements[2] * elements[15] + 
	         elements[8]  * elements[3] * elements[14] + 
	         elements[12] * elements[2] * elements[11] - 
	         elements[12] * elements[3] * elements[10];
	inv[9] = -elements[0]  * elements[9] * elements[15] + 
	          elements[0]  * elements[11] * elements[13] + 
	          elements[8]  * elements[1] * elements[15] - 
	          elements[8]  * elements[3] * elements[13] - 
	          elements[12] * elements[1] * elements[11] + 
	          elements[12] * elements[3] * elements[9];
	inv[13] = elements[0]  * elements[9] * elements[14] - 
	          elements[0]  * elements[10] * elements[13] - 
	          elements[8]  * elements[1] * elements[14] + 
	          elements[8]  * elements[2] * elements[13] + 
	          elements[12] * elements[1] * elements[10] - 
	          elements[12] * elements[2] * elements[9];
	inv[2] = elements[1]  * elements[6] * elements[15] - 
	         elements[1]  * elements[7] * elements[14] - 
	         elements[5]  * elements[2] * elements[15] + 
	         elements[5]  * elements[3] * elements[14] + 
	         elements[13] * elements[2] * elements[7] - 
	         elements[13] * elements[3] * elements[6];
	inv[6] = -elements[0]  * elements[6] * elements[15] + 
	          elements[0]  * elements[7] * elements[14] + 
	          elements[4]  * elements[2] * elements[15] - 
	          elements[4]  * elements[3] * elements[14] - 
	          elements[12] * elements[2] * elements[7] + 
	          elements[12] * elements[3] * elements[6];
	inv[10] = elements[0]  * elements[5] * elements[15] - 
	          elements[0]  * elements[7] * elements[13] - 
	          elements[4]  * elements[1] * elements[15] + 
	          elements[4]  * elements[3] * elements[13] + 
	          elements[12] * elements[1] * elements[7] - 
	          elements[12] * elements[3] * elements[5];
	inv[14] = -elements[0]  * elements[5] * elements[14] + 
	           elements[0]  * elements[6] * elements[13] + 
	           elements[4]  * elements[1] * elements[14] - 
	           elements[4]  * elements[2] * elements[13] - 
	           elements[12] * elements[1] * elements[6] + 
	           elements[12] * elements[2] * elements[5];
	inv[3] = -elements[1] * elements[6] * elements[11] + 
	          elements[1] * elements[7] * elements[10] + 
	          elements[5] * elements[2] * elements[11] - 
	          elements[5] * elements[3] * elements[10] - 
	          elements[9] * elements[2] * elements[7] + 
	          elements[9] * elements[3] * elements[6];
	inv[7] = elements[0] * elements[6] * elements[11] - 
	         elements[0] * elements[7] * elements[10] - 
	         elements[4] * elements[2] * elements[11] + 
	         elements[4] * elements[3] * elements[10] + 
	         elements[8] * elements[2] * elements[7] - 
	         elements[8] * elements[3] * elements[6];
	inv[11] = -elements[0] * elements[5] * elements[11] + 
	           elements[0] * elements[7] * elements[9] + 
	           elements[4] * elements[1] * elements[11] - 
	           elements[4] * elements[3] * elements[9] - 
	           elements[8] * elements[1] * elements[7] + 
	           elements[8] * elements[3] * elements[5];
	inv[15] = elements[0] * elements[5] * elements[10] - 
	          elements[0] * elements[6] * elements[9] - 
	          elements[4] * elements[1] * elements[10] + 
	          elements[4] * elements[2] * elements[9] + 
	          elements[8] * elements[1] * elements[6] - 
	          elements[8] * elements[2] * elements[5];
	f32 det = elements[0] * inv[0] + elements[1] * inv[4] + 
	          elements[2] * inv[8] + elements[3] * inv[12];
	if (det == 0)
		return false;
	det = 1.0f / det;
	for (u8 i = 0; i < 16; i++)
		o_elements[i] = inv[i] * det;
	return true;
}
inline m4x4f32 m4x4f32::operator*(const m4x4f32& other) const
{
	const m4x4f32 otherTranspose = transpose(other.elements);
	m4x4f32 result = {};
	for(u8 thisRow = 0; thisRow < 4; thisRow++)
	{
		for(u8 otherCol = 0; otherCol < 4; otherCol++)
		{
			const v4f32* row = reinterpret_cast<const v4f32*>(
				elements + 4*thisRow);
			const v4f32* col = reinterpret_cast<const v4f32*>(
				otherTranspose.elements + 4*otherCol);
			result.elements[thisRow*4 + otherCol] = row->dot(*col);
		}
	}
	return result;
}
internal v4f32 operator*(const m4x4f32& lhs, const v4f32& rhs)
{
	v4f32 result = {};
	for(u8 r = 0; r < 4; r++)
	{
		result.elements[r] = lhs.elements[4*r + 0]*rhs.elements[0];
		for(u8 c = 1; c < 4; c++)
		{
			result.elements[r] += lhs.elements[4*r + c]*rhs.elements[c];
		}
	}
	return result;
}
internal inline kQuaternion operator*(const kQuaternion& lhs, 
                                      const kQuaternion& rhs)
{
	return kQuaternion::hamilton(lhs, rhs);
}
inline kQuaternion kQuaternion::hamilton(const kQuaternion& q0, 
                                         const kQuaternion& q1)
{
	return { q0.qw*q1.qw - q0.qx*q1.qx - q0.qy*q1.qy - q0.qz*q1.qz, 
	         q0.qw*q1.qx + q0.qx*q1.qw + q0.qy*q1.qz - q0.qz*q1.qy,
	         q0.qw*q1.qy - q0.qx*q1.qz + q0.qy*q1.qw + q0.qz*q1.qx,
	         q0.qw*q1.qz + q0.qx*q1.qy - q0.qy*q1.qx + q0.qz*q1.qw };
}
inline kQuaternion::kQuaternion(f32 w, f32 x, f32 y, f32 z)
{
	this->qw = w;
	this->qx = x;
	this->qy = y;
	this->qz = z;
}
inline kQuaternion::kQuaternion(v3f32 axis, f32 radians, bool axisIsNormalized)
{
	if(!axisIsNormalized)
	{
		axis.normalize();
	}
	const f32 sine = sinf(radians/2);
	qw = cosf(radians/2);
	qx = sine * axis.x;
	qy = sine * axis.y;
	qz = sine * axis.z;
}
inline kQuaternion kQuaternion::conjugate() const
{
	return {qw, -qx, -qy, -qz};
}
inline v2f32 kQuaternion::transform(const v2f32& v2d, bool quatIsNormalized)
{
	if(!quatIsNormalized)
	{
		normalize();
	}
	const kQuaternion result = 
		hamilton(hamilton(*this, {0, v2d.x, v2d.y, 0}), conjugate());
	return {result.qx, result.qy};
}
inline v3f32 kQuaternion::transform(const v3f32& v3d, bool quatIsNormalized)
{
	if(!quatIsNormalized)
	{
		normalize();
	}
	const kQuaternion result = 
		hamilton(hamilton(*this, {0, v3d.x, v3d.y, v3d.z}), conjugate());
	return {result.qx, result.qy, result.qz};
}
internal inline bool kmath::isNearlyEqual(f32 fA, f32 fB, f32 epsilon)
{
	const f32 diff = fabsf(fA - fB);
	fA = fabsf(fA);
	fB = fabsf(fB);
	const f32 largest = (fB > fA) ? fB : fA;
	return (diff <= largest * epsilon);
}
internal inline bool kmath::isNearlyZero(f32 f, f32 epsilon)
{
	return isNearlyEqual(f, 0.f, epsilon);
}
internal inline u64 kmath::kilobytes(u64 k)
{
	return k*1024;
}
internal inline u64 kmath::megabytes(u64 m)
{
	return kilobytes(m)*1024;
}
internal inline u64 kmath::gigabytes(u64 g)
{
	return megabytes(g)*1024;
}
internal inline u64 kmath::terabytes(u64 t)
{
	return gigabytes(t)*1024;
}
internal inline u64 kmath::safeTruncateU64(i64 value)
{
	kassert(value >= 0);
	return static_cast<u64>(value);
}
internal inline u32 kmath::safeTruncateU32(u64 value)
{
	kassert(value <= std::numeric_limits<u32>::max());
	return static_cast<u32>(value);
}
internal inline u32 kmath::safeTruncateU32(i32 value)
{
	kassert(value >= 0);
	return static_cast<u32>(value);
}
internal inline u32 kmath::safeTruncateU32(i64 value)
{
	kassert(value >= 0 && value <= std::numeric_limits<u32>::max());
	return static_cast<u32>(value);
}
internal inline i32 kmath::safeTruncateI32(u64 value)
{
	kassert(value <= static_cast<u64>(std::numeric_limits<i32>::max()));
	return static_cast<i32>(value);
}
internal inline u16 kmath::safeTruncateU16(i32 value)
{
	kassert(value >= 0 && value <= std::numeric_limits<u16>::max());
	return static_cast<u16>(value);
}
internal inline u16 kmath::safeTruncateU16(i64 value)
{
	kassert(value >= 0 && value <= std::numeric_limits<u16>::max());
	return static_cast<u16>(value);
}
internal inline u16 kmath::safeTruncateU16(u32 value)
{
	kassert(value <= std::numeric_limits<u16>::max());
	return static_cast<u16>(value);
}
internal inline u16 kmath::safeTruncateU16(u64 value)
{
	kassert(value <= std::numeric_limits<u16>::max());
	return static_cast<u16>(value);
}
internal inline i16 kmath::safeTruncateI16(i32 value)
{
	kassert(value <= std::numeric_limits<i16>::max() && 
	        value >= std::numeric_limits<i16>::min());
	return static_cast<i16>(value);
}
internal inline u8 kmath::safeTruncateU8(u64 value)
{
	kassert(value <= std::numeric_limits<u8>::max());
	return static_cast<u8>(value);
}
internal inline f32 kmath::v2Radians(const v2f32& v)
{
	if(isNearlyZero(v.x) && isNearlyZero(v.y))
	{
		return 0;
	}
	return atan2f(v.y, v.x);
}
internal inline f32 kmath::radiansBetween(
	v2f32 v0, v2f32 v1, bool v0IsNormalized, bool v1IsNormalized)
{
	if(!v0IsNormalized)
	{
		v0.normalize();
	}
	if(!v1IsNormalized)
	{
		v1.normalize();
	}
	f32 dot = v0.dot(v1);
	// It should be mathematically impossible for the dot product to be 
	//	outside the range of [-1,1] since the vectors are normalized, but I 
	//	have to account for weird floating-point error shenanigans here.
	// I also have to handle the case where the caller claimed that a 
	//	vector was normalized, when actually it wasn't, which would cause us 
	//	to go far beyond this range!
	if(dot < -1)
	{
		kassert(isNearlyEqual(dot, -1));
		dot = -1;
	}
	if(dot > 1)
	{
		kassert(isNearlyEqual(dot, 1));
		dot = 1;
	}
	return acosf(dot);
}
internal inline f32 kmath::radiansBetween(
	v3f32 v0, v3f32 v1, bool v0IsNormalized, bool v1IsNormalized)
{
	if(!v0IsNormalized)
	{
		v0.normalize();
	}
	if(!v1IsNormalized)
	{
		v1.normalize();
	}
	f32 dot = v0.dot(v1);
	// It should be mathematically impossible for the dot product to be 
	//	outside the range of [-1,1] since the vectors are normalized, but I 
	//	have to account for weird floating-point error shenanigans here.
	// I also have to handle the case where the caller claimed that a 
	//	vector was normalized, when actually it wasn't, which would cause us 
	//	to go far beyond this range!
	if(dot < -1)
	{
		kassert(isNearlyEqual(dot, -1));
		dot = -1;
	}
	if(dot > 1)
	{
		kassert(isNearlyEqual(dot, 1));
		dot = 1;
	}
	return acosf(dot);
}
internal inline v3f32 kmath::cross(const v2f32& lhs, const v2f32& rhs)
{
	return {0, 0, lhs.x*rhs.y - lhs.y*rhs.x};
}
internal inline v2f32 kmath::normal(v2f32 v)
{
	v.normalize();
	return v;
}
internal inline v3f32 kmath::normal(v3f32 v)
{
	v.normalize();
	return v;
}
internal inline f32 kmath::lerp(f32 min, f32 max, f32 ratio)
{
	return min + ratio*(max - min);
}
internal inline v2f32 kmath::rotate(const v2f32& v, f32 radians)
{
	v2f32 result = 
		{ cosf(radians)*v.x - sinf(radians)*v.y
		, sinf(radians)*v.x + cosf(radians)*v.y
	};
	return result;
}
internal inline u8 kmath::solveQuadratic(f32 a, f32 b, f32 c, f32 o_roots[2])
{
	/* https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-sphere-intersection */
	const f32 discriminant = b*b - 4*a*c;
	if(discriminant < 0)
		/* no real# solution */
		return 0;
	if(isNearlyZero(a))
	/* degenerate line equation case */
	{
		if(isNearlyZero(b))
		/* horizontal line case: either NO SOLUTION or INFINITE */
		{
			if(isNearlyZero(c))
				o_roots[0] = INFINITY32;// super-rare case
			else
				o_roots[0] = 0;
			return 0;
		}
		o_roots[0] = -c / b;
		return 1;
	}
	u8 result = 1;
	if(isNearlyZero(discriminant))
	/* rare case: only ONE real# solution */
	{
		o_roots[0] = -0.5f * b / a;
	}
	else
	{
		result = 2;
		const f32 q = b > 0
			? -0.5f * (b + sqrtf(discriminant))
			: -0.5f * (b - sqrtf(discriminant));
		/* given the cases handled above, it should not be possible for q to 
			approach zero, but just in case let's check that here... */
		kassert(!isNearlyZero(q));
		o_roots[0] = q/a;
		o_roots[1] = c/q;
		if(o_roots[0] > o_roots[1])
		{
			const f32 temp = o_roots[0];
			o_roots[0] = o_roots[1];
			o_roots[1] = temp;
		}
	}
	return result;
}
internal inline f32 kmath::collideRayPlane(
	const v3f32& rayOrigin, const v3f32& rayNormal, 
	const v3f32& planeNormal, f32 planeDistanceFromOrigin, 
	bool cullPlaneBackFace)
{
#if INTERNAL_BUILD
	kassert(isNearlyEqual(rayNormal  .magnitudeSquared(), 1));
	kassert(isNearlyEqual(planeNormal.magnitudeSquared(), 1));
#endif// INTERNAL_BUILD
	const v3f32 planeOrigin = planeDistanceFromOrigin*planeNormal;
	const f32 denominator   = rayNormal.dot(planeNormal);
	if(isNearlyZero(denominator))
	/* the ray is PERPENDICULAR to the plane; no real solution */
	{
		/* test if the ray is co-planar with the plane */
		const v3f32 rayOriginOntoPlane = rayOrigin.projectOnto(planeOrigin);
		if(    isNearlyEqual(rayOriginOntoPlane.x, rayOrigin.x)
		    && isNearlyEqual(rayOriginOntoPlane.y, rayOrigin.y)
		    && isNearlyEqual(rayOriginOntoPlane.z, rayOrigin.z))
		{
			return INFINITY32;
		}
		/* otherwise, they will never intersect */
		return NAN32;
	}
	if(cullPlaneBackFace && rayNormal.dot(planeNormal) >= 0)
	/* if we're colliding with the BACK of the plane (opposite side that the 
		normal is facing) and the caller doesn't want these collisions, then the 
		collision fails here */
	{
		return NAN32;
	}
	const f32 result = (planeOrigin - rayOrigin).dot(planeNormal) / denominator;
	/* if the result is negative, then we are colliding with the ray's line, NOT 
		the ray itself! */
	if(result < 0)
		return NAN32;
	return result;
}
internal inline f32 kmath::collideRaySphere(
		const v3f32& rayOrigin, const v3f32& rayNormal, 
		const v3f32& sphereOrigin, f32 sphereRadius)
{
	/* analytic solution.  Source:
		https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-sphere-intersection */
	const v3f32 l = rayOrigin - sphereOrigin;
	const f32 a = rayNormal.dot(rayNormal);
	const f32 b = 2 * rayNormal.dot(l);
	const f32 c = l.dot(l) - (sphereRadius*sphereRadius);
	f32 analyticSolutions[2];
	const u8 analyticSolutionCount = solveQuadratic(a, b, c, analyticSolutions);
	/* no quadratic solutions means the ray's LINE never collides with the 
		sphere */
	if(analyticSolutionCount == 0)
		return NAN32;
	/* rare case: the ray intersects the sphere at a SINGLE point */
	if(analyticSolutionCount == 1)
		if(analyticSolutions[0] < 0)
			return NAN32;/* intersection is on the wrong side of the ray */
		else
			return analyticSolutions[0];
	/* if the first solution is < 0, the sphere either intersects the ray's 
		origin, or the sphere is colliding with the ray's line BEHIND the ray 
		(no collision with the ray itself) */
	if(analyticSolutions[0] < 0)
		if(analyticSolutions[1] < 0)
			return NAN32;
		else
			return 0;
	else
		return analyticSolutions[0];
}
internal inline void kmath::generateMeshBox(
		v3f32 lengths, GeneratedMeshVertex* o_buffer, size_t bufferByteSize)
{
	kassert(bufferByteSize >= sizeof(GeneratedMeshVertex)*36);
	lengths /= 2.f;
	// top (+Z) low-right
	o_buffer[ 0] = {{ lengths.x, lengths.y, lengths.z}, {1,1}};
	o_buffer[ 1] = {{-lengths.x, lengths.y, lengths.z}, {1,0}};
	o_buffer[ 2] = {{ lengths.x,-lengths.y, lengths.z}, {0,1}};
	// top (+Z) up-left
	o_buffer[ 3] = {{-lengths.x,-lengths.y, lengths.z}, {0,0}};
	o_buffer[ 4] = {{ lengths.x,-lengths.y, lengths.z}, {0,1}};
	o_buffer[ 5] = {{-lengths.x, lengths.y, lengths.z}, {1,0}};
	// front (+X) low-right
	o_buffer[ 6] = {{ lengths.x, lengths.y, lengths.z}, {1,0}};
	o_buffer[ 7] = {{ lengths.x,-lengths.y,-lengths.z}, {0,1}};
	o_buffer[ 8] = {{ lengths.x, lengths.y,-lengths.z}, {1,1}};
	// front (+X) up-left
	o_buffer[ 9] = {{ lengths.x,-lengths.y,-lengths.z}, {0,1}};
	o_buffer[10] = {{ lengths.x, lengths.y, lengths.z}, {1,0}};
	o_buffer[11] = {{ lengths.x,-lengths.y, lengths.z}, {0,0}};
	// left (+Y) low-right
	o_buffer[12] = {{ lengths.x, lengths.y,-lengths.z}, {0,1}};
	o_buffer[13] = {{-lengths.x, lengths.y,-lengths.z}, {1,1}};
	o_buffer[14] = {{-lengths.x, lengths.y, lengths.z}, {1,0}};
	// left (+Y) up-left
	o_buffer[15] = {{-lengths.x, lengths.y, lengths.z}, {1,0}};
	o_buffer[16] = {{ lengths.x, lengths.y, lengths.z}, {0,0}};
	o_buffer[17] = {{ lengths.x, lengths.y,-lengths.z}, {0,1}};
	// back (-X) low-right
	o_buffer[18] = {{-lengths.x, lengths.y,-lengths.z}, {0,1}};
	o_buffer[19] = {{-lengths.x,-lengths.y,-lengths.z}, {1,1}};
	o_buffer[20] = {{-lengths.x,-lengths.y, lengths.z}, {1,0}};
	// back (-X) up-left
	o_buffer[21] = {{-lengths.x,-lengths.y, lengths.z}, {1,0}};
	o_buffer[22] = {{-lengths.x, lengths.y, lengths.z}, {0,0}};
	o_buffer[23] = {{-lengths.x, lengths.y,-lengths.z}, {0,1}};
	// right (-Y) low-right
	o_buffer[24] = {{-lengths.x,-lengths.y,-lengths.z}, {0,1}};
	o_buffer[25] = {{ lengths.x,-lengths.y,-lengths.z}, {1,1}};
	o_buffer[26] = {{ lengths.x,-lengths.y, lengths.z}, {1,0}};
	// right (-Y) up-left
	o_buffer[27] = {{ lengths.x,-lengths.y, lengths.z}, {1,0}};
	o_buffer[28] = {{-lengths.x,-lengths.y, lengths.z}, {0,0}};
	o_buffer[29] = {{-lengths.x,-lengths.y,-lengths.z}, {0,1}};
	// bottom (-Z) low-right
	o_buffer[30] = {{-lengths.x,-lengths.y,-lengths.z}, {0,1}};
	o_buffer[31] = {{-lengths.x, lengths.y,-lengths.z}, {1,1}};
	o_buffer[32] = {{ lengths.x, lengths.y,-lengths.z}, {1,0}};
	// bottom (-Z) up-left
	o_buffer[33] = {{ lengths.x, lengths.y,-lengths.z}, {1,0}};
	o_buffer[34] = {{ lengths.x,-lengths.y,-lengths.z}, {0,0}};
	o_buffer[35] = {{-lengths.x,-lengths.y,-lengths.z}, {0,1}};
}
internal inline size_t kmath::generateMeshCircleSphereVertexCount(
		u32 latitudeSegments, u32 longitudeSegments)
{
	return ((latitudeSegments - 2)*6 + 6) * longitudeSegments;
}
internal inline void kmath::generateMeshCircleSphere(
		f32 radius, u32 latitudeSegments, u32 longitudeSegments, 
		GeneratedMeshVertex* o_buffer, size_t bufferByteSize)
{
	const size_t requiredVertexCount = 
		generateMeshCircleSphereVertexCount(
			latitudeSegments, longitudeSegments);
	kassert(bufferByteSize >= requiredVertexCount*sizeof(GeneratedMeshVertex));
	kassert(latitudeSegments  >= 2);
	kassert(longitudeSegments >= 3);
	const f32 radiansPerSemiLongitude = 2*PI32 / longitudeSegments;
	const f32 radiansPerLatitude      = PI32 / latitudeSegments;
	const v3f32 verticalRadius        = v3f32::Z * radius;
	size_t currentVertex = 0;
	for(u32 longitude = 1; longitude <= longitudeSegments; longitude++)
	{
		/* add the next triangle to the top cap */
		const v3f32 capTopZeroLongitude = 
			kQuaternion(v3f32::Y, radiansPerLatitude, true)
				.transform(verticalRadius, true);
		kQuaternion quatLongitude(
			v3f32::Z, longitude*radiansPerSemiLongitude, true);
		const v3f32 capTopLongitudeCurrent = 
			quatLongitude.transform(capTopZeroLongitude, true);
		kQuaternion quatLongitudePrevious(
			v3f32::Z, (longitude - 1)*radiansPerSemiLongitude, true);
		const v3f32 capTopLongitudePrevious = 
			quatLongitudePrevious.transform(capTopZeroLongitude, true);
		o_buffer[currentVertex++] = {capTopLongitudePrevious, {}};
		o_buffer[currentVertex++] = {capTopLongitudeCurrent , {}};
		o_buffer[currentVertex++] = {verticalRadius         , {}};
		/* add the next triangle to the bottom  cap*/
		o_buffer[currentVertex  ] = {capTopLongitudeCurrent , {}};
		o_buffer[currentVertex++].localPosition.z *= -1;
		o_buffer[currentVertex  ] = {capTopLongitudePrevious, {}};
		o_buffer[currentVertex++].localPosition.z *= -1;
		o_buffer[currentVertex++] = {v3f32::Z*-radius       , {}};
		/* add the quads to the internal latitude strips */
		for(u32 latitude = 1; latitude < latitudeSegments - 1; latitude++)
		{
			kQuaternion quatLatitudePrevious(
				v3f32::Y, latitude*radiansPerLatitude, true);
			kQuaternion quatLatitude(
				v3f32::Y, (latitude + 1)*radiansPerLatitude, true);
			const v3f32 latStripTopPrevious = 
				(quatLongitudePrevious * quatLatitudePrevious)
					.transform(verticalRadius, true);
			const v3f32 latStripBottomPrevious = 
				(quatLongitudePrevious * quatLatitude)
					.transform(verticalRadius, true);
			const v3f32 latStripTop = (quatLongitude * quatLatitudePrevious)
				.transform(verticalRadius, true);
			const v3f32 latStripBottom = (quatLongitude * quatLatitude)
				.transform(verticalRadius, true);
			o_buffer[currentVertex++] = {latStripTopPrevious   , {}};
			o_buffer[currentVertex++] = {latStripBottomPrevious, {}};
			o_buffer[currentVertex++] = {latStripBottom        , {}};
			o_buffer[currentVertex++] = {latStripBottom        , {}};
			o_buffer[currentVertex++] = {latStripTop           , {}};
			o_buffer[currentVertex++] = {latStripTopPrevious   , {}};
		}
	}
	kassert(currentVertex == requiredVertexCount);
	/* calculate proper texture normals based on cylindrical projection 
		Source: https://gamedev.stackexchange.com/a/114416 */
	/* @TODO: figure out how to improve this at some point?  it seems very 
		glitchy on the north & south poles, as well as some other issues with 
		low resolution spheres...  But I don't care about generated sphere 
		textures right now so... */
	for(size_t v = 0; v < requiredVertexCount; v++)
	{
		const v3f32 positionNorm = normal(o_buffer[v].localPosition);
		o_buffer[v].textureNormal.x = 1.f - 
			(atan2f(positionNorm.x, positionNorm.y) / (2*PI32) + 0.5f);
		o_buffer[v].textureNormal.y = 1.f - (positionNorm.z * 0.5f + 0.5f);
	}
}