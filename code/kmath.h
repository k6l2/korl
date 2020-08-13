#pragma once
#include "kutil.h"
const f32 PI32 = 3.14159f;
// crt math operations
#include <math.h>
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
public:
	inline f32 magnitude() const;
	inline f32 magnitudeSquared() const;
	/** @return the magnitude of the vector before normalization */
	inline f32 normalize();
};
struct v4f32
{
	union 
	{
		f32 elements[4];
		struct
		{
			f32 w, x, y, z;
		};
	};
public:
	inline f32 magnitude() const;
	inline f32 magnitudeSquared() const;
	/** @return the magnitude of the vector before normalization */
	inline f32 normalize();
};
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
	class_namespace m4x4f32 transpose(const f32* elements);
};
struct kQuaternion : public v4f32
{
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
};
namespace kmath
{
	global_variable const kQuaternion IDENTITY_QUATERNION = {1,0,0,0};
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
	internal inline u16 safeTruncateU16(u32 value);
	internal inline u16 safeTruncateU16(u64 value);
	internal inline i16 safeTruncateI16(i32 value);
	internal inline u8 safeTruncateU8(u64 value);
	internal inline f32 v2Radians(const v2f32& v);
	internal inline f32 radiansBetween(v2f32 v0, v2f32 v1, 
	                                   bool v0IsNormalized = false, 
	                                   bool v1IsNormalized = false);
	internal inline v3f32 cross(const v2f32& lhs, const v2f32& rhs);
	internal inline v2f32 normal(v2f32 v);
	internal inline f32 lerp(f32 min, f32 max, f32 ratio);
	internal inline v2f32 rotate(const v2f32& v, f32 radians);
}
internal inline v2f32 operator*(f32 lhs, const v2f32& rhs);
internal v4f32 operator*(const m4x4f32& lhs, const v4f32& rhs);
internal inline kQuaternion operator*(const kQuaternion& lhs, 
                                      const kQuaternion& rhs);