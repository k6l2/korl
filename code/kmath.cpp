#include "kmath.h"
#include "kgt-math-internal.h"
#include <limits>
const v2f32 v2f32::ZERO = {0,0};
const v3f32 v3f32::ZERO = {0,0,0};
const v3f32 v3f32::X = {1,0,0};
const v3f32 v3f32::Y = {0,1,0};
const v3f32 v3f32::Z = {0,0,1};
const v4f32 v4f32::ZERO = {0,0,0,0};
const q32 q32::IDENTITY = {1,0,0,0};
const m4x4f32 m4x4f32::IDENTITY = {1,0,0,0,
                                   0,1,0,0,
                                   0,0,1,0,
                                   0,0,0,1};
inline v2u32 v2u32::operator/(u32 discreteScalar) const
{
	korlAssert(discreteScalar != 0);
	return {x / discreteScalar, y / discreteScalar};
}
inline v2u32 v2u32::operator*(const v2u32& other) const
{
	return {x*other.x, y*other.y};
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
inline bool v2f32::isNearlyZero() const
{
	return kmath::isNearlyZero(x)
	    && kmath::isNearlyZero(y);
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
inline v3f32 v3f32::operator-() const
{
	return {-1*x, -1*y, -1*z};
}
internal inline v3f32 operator*(f32 lhs, const v3f32& rhs)
{
	return {lhs*rhs.x, lhs*rhs.y, lhs*rhs.z};
}
internal inline v3f32 operator/(const v3f32& lhs, f32 rhs)
{
	return {lhs.x / rhs, lhs.y / rhs, lhs.z / rhs};
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
inline v3f32& v3f32::operator*=(f32 scalar)
{
	x *= scalar;
	y *= scalar;
	z *= scalar;
	return *this;
}
inline v3f32& v3f32::operator/=(f32 scalar)
{
	korlAssert(!kmath::isNearlyZero(scalar));
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
inline bool v3f32::isNearlyZero() const
{
	return kmath::isNearlyZero(x)
	    && kmath::isNearlyZero(y)
	    && kmath::isNearlyZero(z);
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
	x *= scalar;
	y *= scalar;
	z *= scalar;
	w *= scalar;
	return *this;
}
inline f32 v4f32::magnitude() const
{
	return sqrtf(powf(w,2) + powf(x,2) + powf(y,2) + powf(z,2));
}
inline f32 v4f32::magnitudeSquared() const
{
	return powf(w,2) + powf(x,2) + powf(y,2) + powf(z,2);
}
inline f32 v4f32::normalize()
{
	const f32 mag = magnitude();
	if(kmath::isNearlyZero(mag))
	{
		return w = x = y = z = 0;
	}
	w /= mag;
	x /= mag;
	y /= mag;
	z /= mag;
	return mag;
}
inline f32 v4f32::dot(const v4f32& other) const
{
	return w*other.w + x*other.x + y*other.y + z*other.z;
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
internal inline q32 operator*(const q32& lhs, const q32& rhs)
{
	return q32::hamilton(lhs, rhs);
}
inline q32 q32::hamilton(const q32& q0, const q32& q1)
{
	return { q0.qw*q1.qw - q0.qx*q1.qx - q0.qy*q1.qy - q0.qz*q1.qz, 
	         q0.qw*q1.qx + q0.qx*q1.qw + q0.qy*q1.qz - q0.qz*q1.qy,
	         q0.qw*q1.qy - q0.qx*q1.qz + q0.qy*q1.qw + q0.qz*q1.qx,
	         q0.qw*q1.qz + q0.qx*q1.qy - q0.qy*q1.qx + q0.qz*q1.qw };
}
inline q32::q32(f32 w, f32 x, f32 y, f32 z)
{
	this->qw = w;
	this->qx = x;
	this->qy = y;
	this->qz = z;
}
inline q32::q32(v3f32 axis, f32 radians, bool axisIsNormalized)
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
inline q32 q32::conjugate() const
{
	return {qw, -qx, -qy, -qz};
}
inline v2f32 q32::transform(const v2f32& v2d, bool quatIsNormalized)
{
	if(!quatIsNormalized)
	{
		normalize();
	}
	const q32 result = 
		hamilton(hamilton(*this, {0, v2d.x, v2d.y, 0}), conjugate());
	return {result.qx, result.qy};
}
inline v3f32 q32::transform(const v3f32& v3d, bool quatIsNormalized)
{
	if(!quatIsNormalized)
	{
		normalize();
	}
	const q32 result = 
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
internal inline f32 kmath::min(f32 a, f32 b)
{
	return (a < b ? a : b);
}
internal inline size_t kmath::min(size_t a, size_t b)
{
	return (a < b ? a : b);
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
	korlAssert(value >= 0);
	return static_cast<u64>(value);
}
internal inline u32 kmath::safeTruncateU32(u64 value)
{
	korlAssert(value <= std::numeric_limits<u32>::max());
	return static_cast<u32>(value);
}
internal inline u32 kmath::safeTruncateU32(i32 value)
{
	korlAssert(value >= 0);
	return static_cast<u32>(value);
}
internal inline u32 kmath::safeTruncateU32(i64 value)
{
	korlAssert(value >= 0 && value <= std::numeric_limits<u32>::max());
	return static_cast<u32>(value);
}
internal inline i32 kmath::safeTruncateI32(u64 value)
{
	korlAssert(value <= static_cast<u64>(std::numeric_limits<i32>::max()));
	return static_cast<i32>(value);
}
internal inline u16 kmath::safeTruncateU16(i32 value)
{
	korlAssert(value >= 0 && value <= std::numeric_limits<u16>::max());
	return static_cast<u16>(value);
}
internal inline u16 kmath::safeTruncateU16(i64 value)
{
	korlAssert(value >= 0 && value <= std::numeric_limits<u16>::max());
	return static_cast<u16>(value);
}
internal inline u16 kmath::safeTruncateU16(u32 value)
{
	korlAssert(value <= std::numeric_limits<u16>::max());
	return static_cast<u16>(value);
}
internal inline u16 kmath::safeTruncateU16(u64 value)
{
	korlAssert(value <= std::numeric_limits<u16>::max());
	return static_cast<u16>(value);
}
internal inline i16 kmath::safeTruncateI16(i32 value)
{
	korlAssert(value <= std::numeric_limits<i16>::max() && 
	           value >= std::numeric_limits<i16>::min());
	return static_cast<i16>(value);
}
internal inline u8 kmath::safeTruncateU8(i32 value)
{
	korlAssert(value >= 0 
	        && value <= std::numeric_limits<u8>::max());
	return static_cast<u8>(value);
}
internal inline u8 kmath::safeTruncateU8(u64 value)
{
	korlAssert(value <= std::numeric_limits<u8>::max());
	return static_cast<u8>(value);
}
internal inline i8 kmath::safeTruncateI8(i32 value)
{
	korlAssert(value <= std::numeric_limits<i8>::max() 
	        && value >= std::numeric_limits<i8>::min());
	return static_cast<i8>(value);
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
		korlAssert(isNearlyEqual(dot, -1));
		dot = -1;
	}
	if(dot > 1)
	{
		korlAssert(isNearlyEqual(dot, 1));
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
		korlAssert(isNearlyEqual(dot, -1));
		dot = -1;
	}
	if(dot > 1)
	{
		korlAssert(isNearlyEqual(dot, 1));
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
internal inline v3f32 kmath::normal(v3f32 p0, v3f32 p1, v3f32 p2)
{
	p1 = p1 - p0;
	p2 = p2 - p0;
	p0 = p1.cross(p2);
	p0.normalize();
	return p0;
}
internal inline v3f32 kmath::unNormal(
	const v3f32& p0, const v3f32& p1, const v3f32& p2)
{
	return (p1 - p0).cross(p2 - p0);
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
internal bool kmath::coplanar(
	const v3f32& p0, const v3f32& p1, const v3f32& p2, const v3f32& p3)
{
	/* 4 points can be defined as 3 vectors eminating from a single point */
	const v3f32 v0 = p1 - p0;
	const v3f32 v1 = p2 - p0;
	const v3f32 v2 = p3 - p0;
	/* the points are coplanar if one of the vectors is exactly 
	 * perpendicular to the normal of the plane defined by the first two */
	return isNearlyZero(v2.dot(v0.cross(v1)));
}
internal inline void kmath::makeM4f32(const q32& q, m4x4f32* o_m)
{
	/* https://www.euclideanspace.com/maths/geometry/rotations/conversions/quaternionToMatrix */
	const f32 sqW = q.qw*q.qw;
	const f32 sqX = q.qx*q.qx;
	const f32 sqY = q.qy*q.qy;
	const f32 sqZ = q.qz*q.qz;
	const f32 inverseSquareLength = 1 / (sqW + sqX + sqY + sqZ);
	o_m->r0c0 = ( sqX - sqY - sqZ + sqW) * inverseSquareLength;
	o_m->r1c1 = (-sqX + sqY - sqZ + sqW) * inverseSquareLength;
	o_m->r2c2 = (-sqX - sqY + sqZ + sqW) * inverseSquareLength;
	const f32 temp1 = q.qx*q.qy;
	const f32 temp2 = q.qz*q.qw;
	o_m->r1c0 = 2*(temp1 + temp2) * inverseSquareLength;
	o_m->r0c1 = 2*(temp1 - temp2) * inverseSquareLength;
	const f32 temp3 = q.qx*q.qz;
	const f32 temp4 = q.qy*q.qw;
	o_m->r2c0 = 2*(temp3 - temp4) * inverseSquareLength;
	o_m->r0c2 = 2*(temp3 + temp4) * inverseSquareLength;
	const f32 temp5 = q.qy*q.qz;
	const f32 temp6 = q.qx*q.qw;
	o_m->r2c1 = 2*(temp5 + temp6) * inverseSquareLength;
	o_m->r1c2 = 2*(temp5 - temp6) * inverseSquareLength;
	o_m->r0c3 = o_m->r1c3 = o_m->r2c3 = 0;
	o_m->r3c0 = o_m->r3c1 = o_m->r3c2 = 0;
	o_m->r3c3 = 1;
}
internal inline void kmath::makeM4f32(
		const q32& q, const v3f32& translation, m4x4f32* o_m)
{
	m4x4f32 m4Rotation;
	makeM4f32(q, &m4Rotation);
	m4x4f32 m4Translation = m4x4f32::IDENTITY;
	m4Translation.r0c3 = translation.x;
	m4Translation.r1c3 = translation.y;
	m4Translation.r2c3 = translation.z;
	*o_m = m4Translation * m4Rotation;
}
internal inline f32 kmath::sine_0_1(f32 radians)
{
	return 0.5f*sinf(radians) + 0.5f;
}
internal inline f32 kmath::clamp(f32 x, f32 min, f32 max)
{
	if(x < min)
		return min;
	if(x > max)
		return max;
	return x;
}
internal inline f32 kmath::max(f32 a, f32 b)
{
	if(a >= b)
		return a;
	return b;
}
internal inline u32 kmath::max(u32 a, u32 b)
{
	if(a >= b)
		return a;
	return b;
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
		korlAssert(!isNearlyZero(q));
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
	korlAssert(isNearlyEqual(rayNormal  .magnitudeSquared(), 1));
	korlAssert(isNearlyEqual(planeNormal.magnitudeSquared(), 1));
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
internal inline f32 kmath::collideRayBox(
		const v3f32& rayOrigin, const v3f32& rayNormal, 
		const v3f32& boxWorldPosition, const q32& boxOrientation, 
		const v3f32& boxLengths)
{
	/* geometric solution.  Sources:
		http://www.opengl-tutorial.org/miscellaneous/clicking-on-objects/picking-with-custom-ray-obb-function/ 
		https://svn.science.uu.nl/repos/edu.3803627.INFOMOV/0AD/docs/ray_intersect.pdf */
	m4x4f32 boxModelMatrix;
	makeM4f32(boxOrientation, boxWorldPosition, &boxModelMatrix);
	f32 tMin = 0;
	f32 tMax = INFINITY32;
	const v3f32 boxExtents = boxLengths / 2;
	const v3f32 rayToBox = boxWorldPosition - rayOrigin;
	auto testAxis = [&](u8 axisIndex)->f32
	{
		/* given an arbitrary axis of the box in world space, we can define a 
			`slab` as being the region of points that lies in the range 
			[-axisExtent, axisExtent] where axisExtent==boxExtents[axisIndex] */
		/* we can derive the X,Y & Z axes from a ModelView matrix from the 
			columns of the upper-left corner.  Source:
			http://www.songho.ca/opengl/gl_transform.html#matrix */
		const v3f32 axis = 
			{ boxModelMatrix.elements[axisIndex    ]
			, boxModelMatrix.elements[axisIndex + 4]
			, boxModelMatrix.elements[axisIndex + 8] };
		const f32 axisExtent = boxExtents.elements[axisIndex];
		const f32 scalarProj_rayToBox_axis = axis.dot(rayToBox);
		const f32 cosAngle_axis_rayNormal  = axis.dot(rayNormal);
		if(isNearlyZero(cosAngle_axis_rayNormal))
		/* if the cos of the angle between axis & the ray normal is 0, it means 
			the ray is perpendicular to the slab, which means the ray lies 
			entirely within the slab or entirely outside the slab */
		{
			/* if the ray is outside of the slab */
			if(scalarProj_rayToBox_axis > axisExtent
				/* or the ray is pointing away from the box */
				|| rayNormal.dot(rayToBox) < 0)
				/* we cannot be colliding! */
				return NAN32;
		}
		else
		/* otherwise, we can check to see if the accumulated intersections 
			between the ray and the slab convey the possibility of an 
			intersection with the box using some fancy logic~ (see sources) */
		{
			f32 t1 = (scalarProj_rayToBox_axis - axisExtent) / 
				cosAngle_axis_rayNormal;
			f32 t2 = (scalarProj_rayToBox_axis + axisExtent) / 
				cosAngle_axis_rayNormal;
			if(t1 > t2)
			/* swap to ensure t1 <= t2 */
			{
				f32 temp = t1; t1 = t2; t2 = temp;
			}
			if(t2 < tMax)
				tMax = t2;
			if(t1 > tMin)
				tMin = t1;
			if(tMax < tMin)
				return NAN32;
		}
		return tMin;
	};
	for(u8 axisIndex = 0; axisIndex < 3; axisIndex++)
		if(isnan(testAxis(axisIndex)))
			return NAN32;
	return tMin;
}
#define COLOR4F32_STRIDE(pu8,byteStride,index) \
	reinterpret_cast<Color4f32*>((pu8) + ((index)*(byteStride)))
#define V3F32_STRIDE(pu8,byteStride,index) \
	reinterpret_cast<v3f32*>((pu8) + ((index)*(byteStride)))
#define V2F32_STRIDE(pu8,byteStride,index) \
	reinterpret_cast<v2f32*>((pu8) + ((index*byteStride)))
internal inline void kmath::generateMeshBox(
	v3f32 lengths, void* o_vertexData, size_t vertexDataBytes, 
	size_t vertexByteStride, size_t vertexPositionOffset, 
	size_t vertexTextureNormalOffset)
{
	/* pre-calculate half-lengths from lengths */
	lengths /= 2.f;
	/* @optimization: instead of writing these values to two memory locations 
		for position, just write them once directly into the buffer along with 
		the texture normals to promote good space locality & cut # of writes.  
		I just did it this way to make it easier to read... */
	const v3f32 triPositions[] = 
		// top (+Z) low-right
		{ { lengths.x, lengths.y, lengths.z}
		, {-lengths.x, lengths.y, lengths.z}
		, { lengths.x,-lengths.y, lengths.z}
		// top (+Z) up-left
		, {-lengths.x,-lengths.y, lengths.z}
		, { lengths.x,-lengths.y, lengths.z}
		, {-lengths.x, lengths.y, lengths.z}
		// front (+X) low-right
		, { lengths.x, lengths.y, lengths.z}
		, { lengths.x,-lengths.y,-lengths.z}
		, { lengths.x, lengths.y,-lengths.z}
		// front (+X) up-left
		, { lengths.x,-lengths.y,-lengths.z}
		, { lengths.x, lengths.y, lengths.z}
		, { lengths.x,-lengths.y, lengths.z}
		// left (+Y) low-right
		, { lengths.x, lengths.y,-lengths.z}
		, {-lengths.x, lengths.y,-lengths.z}
		, {-lengths.x, lengths.y, lengths.z}
		// left (+Y) up-left
		, {-lengths.x, lengths.y, lengths.z}
		, { lengths.x, lengths.y, lengths.z}
		, { lengths.x, lengths.y,-lengths.z}
		// back (-X) low-right
		, {-lengths.x, lengths.y,-lengths.z}
		, {-lengths.x,-lengths.y,-lengths.z}
		, {-lengths.x,-lengths.y, lengths.z}
		// back (-X) up-left
		, {-lengths.x,-lengths.y, lengths.z}
		, {-lengths.x, lengths.y, lengths.z}
		, {-lengths.x, lengths.y,-lengths.z}
		// right (-Y) low-right
		, {-lengths.x,-lengths.y,-lengths.z}
		, { lengths.x,-lengths.y,-lengths.z}
		, { lengths.x,-lengths.y, lengths.z}
		// right (-Y) up-left
		, { lengths.x,-lengths.y, lengths.z}
		, {-lengths.x,-lengths.y, lengths.z}
		, {-lengths.x,-lengths.y,-lengths.z}
		// bottom (-Z) low-right
		, {-lengths.x,-lengths.y,-lengths.z}
		, {-lengths.x, lengths.y,-lengths.z}
		, { lengths.x, lengths.y,-lengths.z}
		// bottom (-Z) up-left
		, { lengths.x, lengths.y,-lengths.z}
		, { lengths.x,-lengths.y,-lengths.z}
		, {-lengths.x,-lengths.y,-lengths.z} };
	local_persist const v2f32 TRI_TEX_NORMS[] = 
		// top (+Z) low-right, up-left
		{ {1,1}, {1,0}, {0,1}, {0,0}, {0,1}, {1,0}
		// front (+X) low-right, up-left
		, {1,0}, {0,1}, {1,1}, {0,1}, {1,0}, {0,0}
		// left (+Y) low-right, up-left
		, {0,1}, {1,1}, {1,0}, {1,0}, {0,0}, {0,1}
		// back (-X) low-right, up-left
		, {0,1}, {1,1}, {1,0}, {1,0}, {0,0}, {0,1}
		// right (-Y) low-right, up-left
		, {0,1}, {1,1}, {1,0}, {1,0}, {0,0}, {0,1}
		// bottom (-Z) low-right, up-left
		, {0,1}, {1,1}, {1,0}, {1,0}, {0,0}, {0,1} };
	local_persist const size_t requiredVertexCount = CARRAY_SIZE(triPositions);
	static_assert(requiredVertexCount == CARRAY_SIZE(TRI_TEX_NORMS));
	/* ensure that the data buffer passed to us contains enough memory */
	u8*const o_vertexDataU8 = reinterpret_cast<u8*>(o_vertexData);
	u8*const o_positions  = o_vertexDataU8 + vertexPositionOffset;
	u8*const o_texNormals = o_vertexDataU8 + vertexTextureNormalOffset;
	/* sizeof(position) + sizeof(textureNormal) */
	const size_t requiredVertexBytes = sizeof(v3f32) + sizeof(v2f32);
	korlAssert(vertexByteStride >= requiredVertexBytes);
	korlAssert(vertexPositionOffset      <= vertexByteStride - sizeof(v3f32));
	korlAssert(vertexTextureNormalOffset <= vertexByteStride - sizeof(v2f32));
	korlAssert(requiredVertexBytes*requiredVertexCount <= vertexDataBytes);
	/* finally, actually write the data to the output buffer */
	for(size_t v = 0; v < CARRAY_SIZE(triPositions); v++)
	{
		*V3F32_STRIDE(o_positions ,vertexByteStride,v) = triPositions[v];
		*V2F32_STRIDE(o_texNormals,vertexByteStride,v) = TRI_TEX_NORMS[v];
	}
}
internal inline size_t kmath::generateMeshCircleSphereVertexCount(
		u32 latitudeSegments, u32 longitudeSegments)
{
	return ((latitudeSegments - 2)*6 + 6) * longitudeSegments;
}
internal inline void kmath::generateMeshCircleSphere(
		f32 radius, u32 latitudeSegments, u32 longitudeSegments, 
		void* o_vertexData, size_t vertexDataBytes, size_t vertexByteStride, 
		size_t vertexPositionOffset, size_t vertexTextureNormalOffset)
{
	const size_t requiredVertexCount = 
		generateMeshCircleSphereVertexCount(
			latitudeSegments, longitudeSegments);
	/* ensure that the data buffer passed to us contains enough memory */
	u8*const o_vertexDataU8 = reinterpret_cast<u8*>(o_vertexData);
	u8*const o_positions  = o_vertexDataU8 + vertexPositionOffset;
	u8*const o_texNormals = o_vertexDataU8 + vertexTextureNormalOffset;
	/* sizeof(position) + sizeof(textureNormal) */
	const size_t requiredVertexBytes = sizeof(v3f32) + sizeof(v2f32);
	korlAssert(vertexByteStride >= requiredVertexBytes);
	korlAssert(vertexPositionOffset      <= vertexByteStride - sizeof(v3f32));
	korlAssert(vertexTextureNormalOffset <= vertexByteStride - sizeof(v2f32));
	korlAssert(requiredVertexBytes*requiredVertexCount <= vertexDataBytes);
	/* calculate the sphere mesh on-the-fly */
	korlAssert(latitudeSegments  >= 2);
	korlAssert(longitudeSegments >= 3);
	const f32 radiansPerSemiLongitude = 2*PI32 / longitudeSegments;
	const f32 radiansPerLatitude      = PI32 / latitudeSegments;
	const v3f32 verticalRadius        = v3f32::Z * radius;
	size_t currentVertex = 0;
	for(u32 longitude = 1; longitude <= longitudeSegments; longitude++)
	{
		/* add the next triangle to the top cap */
		const v3f32 capTopZeroLongitude = 
			q32(v3f32::Y, radiansPerLatitude, true)
				.transform(verticalRadius, true);
		q32 quatLongitude(
			v3f32::Z, longitude*radiansPerSemiLongitude, true);
		const v3f32 capTopLongitudeCurrent = 
			quatLongitude.transform(capTopZeroLongitude, true);
		q32 quatLongitudePrevious(
			v3f32::Z, (longitude - 1)*radiansPerSemiLongitude, true);
		const v3f32 capTopLongitudePrevious = 
			quatLongitudePrevious.transform(capTopZeroLongitude, true);
		*V3F32_STRIDE(o_positions, vertexByteStride, currentVertex++) = 
			capTopLongitudePrevious;
		*V3F32_STRIDE(o_positions, vertexByteStride, currentVertex++) = 
			capTopLongitudeCurrent;
		*V3F32_STRIDE(o_positions, vertexByteStride, currentVertex++) = 
			verticalRadius;
		/* add the next triangle to the bottom  cap*/
		*V3F32_STRIDE(o_positions, vertexByteStride, currentVertex) = 
			capTopLongitudeCurrent;
		V3F32_STRIDE(o_positions, vertexByteStride, currentVertex++)->z *= -1;
		*V3F32_STRIDE(o_positions, vertexByteStride, currentVertex) = 
			capTopLongitudePrevious;
		V3F32_STRIDE(o_positions, vertexByteStride, currentVertex++)->z *= -1;
		*V3F32_STRIDE(o_positions, vertexByteStride, currentVertex++) = 
			v3f32::Z*-radius;
		/* add the quads to the internal latitude strips */
		for(u32 latitude = 1; latitude < latitudeSegments - 1; latitude++)
		{
			q32 quatLatitudePrevious(
				v3f32::Y, latitude*radiansPerLatitude, true);
			q32 quatLatitude(
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
			*V3F32_STRIDE(o_positions, vertexByteStride, currentVertex++) = 
				latStripTopPrevious;
			*V3F32_STRIDE(o_positions, vertexByteStride, currentVertex++) = 
				latStripBottomPrevious;
			*V3F32_STRIDE(o_positions, vertexByteStride, currentVertex++) = 
				latStripBottom;
			*V3F32_STRIDE(o_positions, vertexByteStride, currentVertex++) = 
				latStripBottom;
			*V3F32_STRIDE(o_positions, vertexByteStride, currentVertex++) = 
				latStripTop;
			*V3F32_STRIDE(o_positions, vertexByteStride, currentVertex++) = 
				latStripTopPrevious;
		}
	}
	korlAssert(currentVertex == requiredVertexCount);
	/* calculate proper texture normals based on cylindrical projection 
		Source: https://gamedev.stackexchange.com/a/114416 */
	/* @TODO: figure out how to improve this at some point?  it seems very 
		glitchy on the north & south poles, as well as some other issues with 
		low resolution spheres...  But I don't care about generated sphere 
		textures right now so... */
	for(size_t v = 0; v < requiredVertexCount; v++)
	{
		const v3f32 positionNorm = 
			normal(*V3F32_STRIDE(o_positions, vertexByteStride, v));
		V2F32_STRIDE(o_texNormals, vertexByteStride, v)->x = 
			1.f - (atan2f(positionNorm.x, positionNorm.y) / (2*PI32) + 0.5f);
		V2F32_STRIDE(o_texNormals, vertexByteStride, v)->y = 
			1.f - (positionNorm.z * 0.5f + 0.5f);
	}
}
internal size_t 
	kmath::generateMeshCircleSphereWireframeVertexCount(
		u32 latitudeSegments, u32 longitudeSegments)
{
	return 2*(longitudeSegments*latitudeSegments + 
	          (longitudeSegments + 1)*(latitudeSegments - 1));
	//return 2 * longitudeSegments * (latitudeSegments + latitudeSegments - 1);
}
internal void 
	kmath::generateMeshCircleSphereWireframe(
		f32 radius, u32 latitudeSegments, u32 longitudeSegments, 
		void* o_vertexData, size_t vertexDataBytes, size_t vertexByteStride, 
		size_t vertexPositionOffset)
{
	const size_t requiredVertexCount = 
		generateMeshCircleSphereWireframeVertexCount(
			latitudeSegments, longitudeSegments);
	/* ensure that the data buffer passed to us contains enough memory */
	u8*const o_vertexDataU8 = reinterpret_cast<u8*>(o_vertexData);
	u8*const o_positions  = o_vertexDataU8 + vertexPositionOffset;
	/* sizeof(position) */
	const size_t requiredVertexBytes = sizeof(v3f32);
	korlAssert(vertexByteStride >= requiredVertexBytes);
	korlAssert(vertexPositionOffset <= vertexByteStride - sizeof(v3f32));
	korlAssert(requiredVertexBytes*requiredVertexCount <= vertexDataBytes);
	/* calculate the sphere mesh on-the-fly */
	korlAssert(latitudeSegments  >= 2);
	korlAssert(longitudeSegments >= 3);
	const f32 radiansPerSemiLongitude = 2*PI32 / longitudeSegments;
	const f32 radiansPerLatitude      =   PI32 / latitudeSegments;
	const v3f32 verticalRadius        = v3f32::Z * radius;
	size_t currentVertex = 0;
	/* build a list of line segments that go from the top of the sphere 
		(+Z) to the bottom (-Z) along this longitude segment, 
		starting at the semi-longitude that faces the +X axis */
	for(u32 longitude = 0; longitude < longitudeSegments; longitude++)
	{
		q32 qLongitude = q32(v3f32::Z, longitude*radiansPerSemiLongitude);
		const v3f32 longitudeRotAxis = qLongitude.transform(v3f32::Y, true);
		for(u32 latitude = 0; latitude < latitudeSegments; latitude++)
		{
			q32 q0 = q32(longitudeRotAxis,  latitude    * radiansPerLatitude);
			q32 q1 = q32(longitudeRotAxis, (latitude+1) * radiansPerLatitude);
			*V3F32_STRIDE(o_positions, vertexByteStride, currentVertex++) = 
				q0.transform(verticalRadius, true);
			*V3F32_STRIDE(o_positions, vertexByteStride, currentVertex++) = 
				q1.transform(verticalRadius, true);
		}
	}
	/* build a list of line segments around each latitude circle */
	for(u32 latitude = 1; latitude < latitudeSegments; latitude++)
	{
		q32 qLatitude = q32(v3f32::Y, latitude*radiansPerLatitude);
		const v3f32 latitudeVector = qLatitude.transform(verticalRadius, true);
		for(u32 longitude = 0; longitude <= longitudeSegments; longitude++)
		{
			q32 q0 = q32(v3f32::Z,  longitude    * radiansPerSemiLongitude);
			q32 q1 = q32(v3f32::Z, (longitude+1) * radiansPerSemiLongitude);
			*V3F32_STRIDE(o_positions, vertexByteStride, currentVertex++) = 
				q0.transform(latitudeVector, true);
			*V3F32_STRIDE(o_positions, vertexByteStride, currentVertex++) = 
				q1.transform(latitudeVector, true);
		}
	}
	korlAssert(currentVertex == requiredVertexCount);
}
internal void kmath::generateUniformSpherePoints(
	u32 pointCount, void* o_vertexData, size_t vertexDataBytes, 
	size_t vertexByteStride, size_t vertexPositionOffset)
{
	/* ensure that the data buffer passed to us contains enough memory */
	u8*const o_vertexDataU8 = reinterpret_cast<u8*>(o_vertexData);
	u8*const o_positions  = o_vertexDataU8 + vertexPositionOffset;
	/* sizeof(position) */
	const size_t requiredVertexBytes = sizeof(v3f32);
	korlAssert(vertexByteStride >= requiredVertexBytes);
	korlAssert(vertexPositionOffset <= vertexByteStride - sizeof(v3f32));
	korlAssert(requiredVertexBytes*pointCount <= vertexDataBytes);
	/* generate the points */
	for(u32 p = 0; p < pointCount; p++)
	{
		const f32 phi   = acosf(1.f - 2.f*(p + 0.5f) / pointCount);
		const f32 theta = 2*PI32 * PHI32 * (p + 0.5f);
		*V3F32_STRIDE(o_positions, vertexByteStride, p) = 
			{cosf(theta)*sinf(phi), sinf(theta)*sinf(phi), cosf(phi)};
	}
}
internal inline v3f32 kmath::supportSphere(
	f32 radius, v3f32 supportDirection, bool supportDirectionIsNormalized)
{
	if(!supportDirectionIsNormalized)
		supportDirection.normalize();
	return radius*supportDirection;
}
internal inline v3f32 kmath::supportBox(
	v3f32 lengths, q32 orientation, v3f32 supportDirection, 
	bool supportDirectionIsNormalized)
{
	if(!supportDirectionIsNormalized)
		supportDirection.normalize();
	/* @optimization: transform the supportDirection by inverse orientation & 
		test with the AABB defined by lengths, then just transform that result 
		by orientation (only 2 transforms instead of 4 required) */
	lengths /= 2;
	orientation.normalize();
	const v3f32 corners[] = 
		{ orientation.transform({lengths.x,  lengths.y,  lengths.z}, true)
		, orientation.transform({lengths.x,  lengths.y, -lengths.z}, true)
		, orientation.transform({lengths.x, -lengths.y, -lengths.z}, true)
		, orientation.transform({lengths.x, -lengths.y,  lengths.z}, true) };
	f32 largestDot = 0;
	u8 farthestCornerIndex = 0;
	for(u8 c = 0; c < CARRAY_SIZE(corners); c++)
	{
		const f32 supportDotCorner = corners[c].dot(supportDirection);
		if(fabsf(supportDotCorner) > fabsf(largestDot))
		{
			largestDot = supportDotCorner;
			farthestCornerIndex = c;
		}
	}
	return corners[farthestCornerIndex]*(largestDot >= 0 ? 1.f : -1.f);
}
internal bool kmath::gjk(
	fnSig_gjkSupport* support, void* supportUserData, v3f32 o_simplex[4], 
	const v3f32* initialSupportDirection)
{
	GjkState gjkState;
	gjk_initialize(&gjkState, support, supportUserData, 
	               initialSupportDirection);
	while(gjkState.lastIterationResult == GjkIterationResult::INCOMPLETE)
	{
		gjk_iterate(&gjkState, support, supportUserData);
		if(gjkState.lastIterationResult == GjkIterationResult::SUCCESS)
		{
			for(u8 s = 0; s < 4; s++)
				o_simplex[s] = gjkState.o_simplex[s];
		}
	}
	return gjkState.lastIterationResult == GjkIterationResult::SUCCESS;
}
internal bool kmath::epa(
	v3f32* o_minimumTranslationNormal, f32* o_minimumTranslationDistance, 
	fnSig_gjkSupport* support, void* supportUserData, const v3f32 simplex[4], 
	KgtAllocatorHandle allocator, f32 resultTolerance)
{
	EpaState state;
	epa_initialize(&state, simplex, allocator, resultTolerance);
	while(state.lastIterationResult == EpaIterationResult::INCOMPLETE)
	{
		epa_iterate(&state, support, supportUserData, allocator);
	}
	if(state.lastIterationResult == EpaIterationResult::SUCCESS)
	{
		*o_minimumTranslationDistance = state.resultDistance;
		*o_minimumTranslationNormal   = state.resultNormal;
	}
	return state.lastIterationResult == EpaIterationResult::SUCCESS;
}
#include "kgt-math-internal.cpp"
