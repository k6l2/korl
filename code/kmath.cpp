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
internal inline void kmath::makeM4f32(const kQuaternion& q, m4x4f32* o_m)
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
		const kQuaternion& q, const v3f32& translation, m4x4f32* o_m)
{
	m4x4f32 m4Rotation;
	makeM4f32(q, &m4Rotation);
	m4x4f32 m4Translation = m4x4f32::IDENTITY;
	m4Translation.r0c3 = translation.x;
	m4Translation.r1c3 = translation.y;
	m4Translation.r2c3 = translation.z;
	*o_m = m4Translation * m4Rotation;
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
internal inline f32 kmath::collideRayBox(
		const v3f32& rayOrigin, const v3f32& rayNormal, 
		const v3f32& boxWorldPosition, const kQuaternion& boxOrientation, 
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
	const size_t requiredVertexCount = CARRAY_SIZE(triPositions);
	kassert(requiredVertexCount == CARRAY_SIZE(TRI_TEX_NORMS));
	/* ensure that the data buffer passed to us contains enough memory */
	u8*const o_vertexDataU8 = reinterpret_cast<u8*>(o_vertexData);
	u8*const o_positions  = o_vertexDataU8 + vertexPositionOffset;
	u8*const o_texNormals = o_vertexDataU8 + vertexTextureNormalOffset;
	/* sizeof(position) + sizeof(textureNormal) */
	const size_t requiredVertexBytes = sizeof(v3f32) + sizeof(v2f32);
	kassert(vertexByteStride >= requiredVertexBytes);
	kassert(vertexPositionOffset      <= vertexByteStride - sizeof(v3f32));
	kassert(vertexTextureNormalOffset <= vertexByteStride - sizeof(v2f32));
	kassert(requiredVertexBytes*requiredVertexCount <= vertexDataBytes);
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
	kassert(vertexByteStride >= requiredVertexBytes);
	kassert(vertexPositionOffset      <= vertexByteStride - sizeof(v3f32));
	kassert(vertexTextureNormalOffset <= vertexByteStride - sizeof(v2f32));
	kassert(requiredVertexBytes*requiredVertexCount <= vertexDataBytes);
	/* calculate the sphere mesh on-the-fly */
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
	kassert(currentVertex == requiredVertexCount);
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
internal void kmath::generateUniformSpherePoints(
	u32 pointCount, void* o_vertexData, size_t vertexDataBytes, 
	size_t vertexByteStride, size_t vertexPositionOffset)
{
	/* ensure that the data buffer passed to us contains enough memory */
	u8*const o_vertexDataU8 = reinterpret_cast<u8*>(o_vertexData);
	u8*const o_positions  = o_vertexDataU8 + vertexPositionOffset;
	/* sizeof(position) */
	const size_t requiredVertexBytes = sizeof(v3f32);
	kassert(vertexByteStride >= requiredVertexBytes);
	kassert(vertexPositionOffset <= vertexByteStride - sizeof(v3f32));
	kassert(requiredVertexBytes*pointCount <= vertexDataBytes);
	/* create a MOSTLY even distribution of points around a unit sphere using 
		the golden ratio.  
		Source: https://stackoverflow.com/a/44164075/4526664 */
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
	v3f32 lengths, kQuaternion orientation, 
	v3f32 supportDirection, bool supportDirectionIsNormalized)
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
internal void kmath::gjk_initialize(
	GjkState* gjkState, fnSig_gjkSupport* support, void* supportUserData, 
	const v3f32* initialSupportDirection)
{
	if(!initialSupportDirection)
		KLOG(WARNING, "Attempting to solve GJK with no initial support "
		     "direction can lead to SIGNIFICANT performance penalties or "
		     "potentially even no answer due to numerical imprecision!");
	const v3f32& initSupportDir = initialSupportDirection 
		? *initialSupportDirection 
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
		const v3f32 newToPrev = o_simplex[0] - newPoint;
		if(newToPrev.dot(newToOrigin) > 0)
		/* origin is closest to the new line segment */
		{
			*o_searchDirection = 
				newToPrev.cross(newToOrigin).cross(newToPrev);
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
	 * wrong due to numerical error and we will most likely never converge! */
	else if(simplexSize == 3 && gjkState->simplexSize == 3 
			&& negativeSimplexUnNorm.dot(gjkState->searchDirection) < 0)
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
	EpaState* epaState, const v3f32 simplex[4], KAllocatorHandle allocator, 
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
	KAllocatorHandle allocator)
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
#if SLOW_BUILD
	kassert(epa_verifyTriListIntegrity(epaState));
#endif // SLOW_BUILD
	hminit(edgeMap, allocator);
	defer(hmfree(edgeMap));
	for(size_t t = 0; t < arrlenu(epaState->tris); t++)
	{
#if SLOW_BUILD
		kassert(epa_verifyTriListIntegrity(epaState));
#endif // SLOW_BUILD
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
#if SLOW_BUILD
			kassert(epa_verifyTriListIntegrity(epaState));
#endif // SLOW_BUILD
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
#if SLOW_BUILD
			kassert(epa_verifyTriListIntegrity(epaState));
#endif // SLOW_BUILD
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
#if SLOW_BUILD
		kassert(epa_verifyTriListIntegrity(epaState));
		//kassert(arrlenu(epaState->tris) < 32);
#endif // SLOW_BUILD
		arrput(epaState->tris, tri);
#if SLOW_BUILD
		kassert(epa_verifyTriListIntegrity(epaState));
#endif // SLOW_BUILD
	}
#if SLOW_BUILD
	kassert(epa_verifyTriListIntegrity(epaState));
#endif // SLOW_BUILD
	arrput(epaState->vertexPositions, supportPoint);
#if SLOW_BUILD
	kassert(epa_verifyTriListIntegrity(epaState));
#endif // SLOW_BUILD
	/* find the next polytope face with the shortest distance to the origin */
	epa_findTriNearestToOrigin(epaState);
	epaState->iteration++;
#if SLOW_BUILD
	kassert(epa_verifyTriListIntegrity(epaState));
#endif // SLOW_BUILD
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
	KAllocatorHandle allocator)
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
internal bool kmath::epa(
	v3f32* o_minimumTranslationNormal, f32* o_minimumTranslationDistance, 
	fnSig_gjkSupport* support, void* supportUserData, const v3f32 simplex[4], 
	KAllocatorHandle allocator, f32 resultTolerance)
{
	EpaState state;
	epa_initialize(&state, simplex, allocator, resultTolerance);
	while(state.lastIterationResult == EpaIterationResult::INCOMPLETE)
	{
		epa_iterate(&state, support, supportUserData, allocator);
#if SLOW_BUILD
		kassert(epa_verifyTriListIntegrity(&state));
#endif // SLOW_BUILD
	}
	if(state.lastIterationResult == EpaIterationResult::SUCCESS)
	{
		*o_minimumTranslationDistance = state.resultDistance;
		*o_minimumTranslationNormal   = state.resultNormal;
	}
	return state.lastIterationResult == EpaIterationResult::SUCCESS;
}
