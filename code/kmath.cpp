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
inline v2f32 operator*(f32 lhs, const v2f32& rhs)
{
	return {lhs*rhs.x, lhs*rhs.y};
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
	return { q0.w*q1.w - q0.x*q1.x - q0.y*q1.y - q0.z*q1.z, 
	         q0.w*q1.x + q0.x*q1.w + q0.y*q1.z - q0.z*q1.y,
	         q0.w*q1.y - q0.x*q1.z + q0.y*q1.w + q0.z*q1.x,
	         q0.w*q1.z + q0.x*q1.y - q0.y*q1.x + q0.z*q1.w };
}
inline kQuaternion::kQuaternion(f32 w, f32 x, f32 y, f32 z)
{
	this->w = w;
	this->x = x;
	this->y = y;
	this->z = z;
}
inline kQuaternion::kQuaternion(v3f32 axis, f32 radians, bool axisIsNormalized)
{
	if(!axisIsNormalized)
	{
		axis.normalize();
	}
	const f32 sine = sinf(radians/2);
	w = cosf(radians/2);
	x = sine * axis.x;
	y = sine * axis.y;
	z = sine * axis.z;
}
inline kQuaternion kQuaternion::conjugate() const
{
	return {w, -x, -y, -z};
}
inline v2f32 kQuaternion::transform(const v2f32& v2d, bool quatIsNormalized)
{
	if(!quatIsNormalized)
	{
		normalize();
	}
	const kQuaternion result = 
		hamilton(hamilton(*this, {0, v2d.x, v2d.y, 0}), conjugate());
	return {result.x, result.y};
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
internal inline f32 kmath::radiansBetween(v2f32 v0, v2f32 v1, 
                                          bool v0IsNormalized, 
                                          bool v1IsNormalized)
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
internal inline f32 kmath::lerp(f32 min, f32 max, f32 ratio)
{
	return min + ratio*(max - min);
}
internal inline v2f32 kmath::rotate(const v2f32& v, f32 radians)
{
	v2f32 result = 
		{ .x = cosf(radians)*v.x - sinf(radians)*v.y
		, .y = sinf(radians)*v.x + cosf(radians)*v.y
	};
	return result;
}