#pragma once
#define internal        static
#define local_persist   static
#define global_variable static
#define class_namespace static
#define CARRAY_COUNT(array) (sizeof(array)/sizeof(array[0]))
// Obtain the file name excluding the path //
//	Source: https://stackoverflow.com/a/8488201
#include <cstring>
#define __FILENAME__ ( strrchr(__FILE__, '\\') \
	? (strrchr(__FILE__, '\\') + 1) \
	: ( (strrchr(__FILE__, '/' ) \
		? strrchr(__FILE__, '/' ) + 1 \
		: __FILE__) ) )
// Defer statement for C++11   //
//	Source: https://www.gingerbill.org/article/2015/08/19/defer-in-cpp/
template <typename F>
struct privDefer {
	F f;
	privDefer(F f) : f(f) {}
	~privDefer() { f(); }
};
template <typename F>
privDefer<F> defer_func(F f) {
	return privDefer<F>(f);
}
#define DEFER_1(x, y) x##y
#define DEFER_2(x, y) DEFER_1(x, y)
#define DEFER_3(x)    DEFER_2(x, __COUNTER__)
#define defer(code)   auto DEFER_3(_defer_) = defer_func([&](){code;})
// custom assert support //
///TODO: improve this to allow platform-specific behavior like trigger debugger?
#if INTERNAL_BUILD || SLOW_BUILD
	#define kassert(expression) do \
	{\
		if(!(expression))\
		{\
			*(int*)0 = 0;\
		}\
	}while(0)
#else
	#define kassert(expression) do {}while(0)
#endif
// These must be re-defined in the appropriate compilation units //
#define KLOG(platformLogCategory, formattedString, ...)
// crt math operations
#include <math.h>
#include <stdint.h>
using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using i8  = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;
using f32 = float;
using f64 = double;
using SoundSample = i16;
const f32 PI32 = 3.14159f;
struct v2f32
{
	f32 x;
	f32 y;
public:
	inline v2f32 operator-() const
	{
		return {-x,-y};
	}
	inline v2f32 operator*(f32 scalar) const
	{
		return {scalar*x, scalar*y};
	}
	inline v2f32 operator/(f32 scalar) const
	{
		return {x/scalar, y/scalar};
	}
	inline v2f32 operator+(const v2f32& other) const
	{
		return {x + other.x, y + other.y};
	}
	inline v2f32 operator-(const v2f32& other) const
	{
		return {x - other.x, y - other.y};
	}
	inline v2f32& operator*=(const f32 scalar)
	{
		x *= scalar;
		y *= scalar;
		return *this;
	}
	inline v2f32& operator+=(const v2f32& other)
	{
		x += other.x;
		y += other.y;
		return *this;
	}
	inline f32 magnitude() const
	{
		return sqrtf(powf(x,2) + powf(y,2));
	}
	inline f32 magnitudeSquared() const
	{
		return powf(x,2) + powf(y,2);
	}
	/** @return the magnitude of the vector before normalization */
	inline f32 normalize();
	inline f32 dot(const v2f32& other) const
	{
		return x*other.x + y*other.y;
	}
	inline v2f32 projectOnto(v2f32 other, bool otherIsNormalized = false) const;
};
inline v2f32 operator*(f32 lhs, const v2f32& rhs)
{
	return {lhs*rhs.x, lhs*rhs.y};
}
struct v3f32
{
	f32 x, y, z;
public:
	inline f32 magnitude() const
	{
		return sqrtf(powf(x,2) + powf(y,2) + powf(z,2));
	}
	inline f32 magnitudeSquared() const
	{
		return powf(x,2) + powf(y,2) + powf(z,2);
	}
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
	inline f32 magnitude() const
	{
		return sqrtf(powf(w,2) + powf(x,2) + powf(y,2) + powf(z,2));
	}
	inline f32 magnitudeSquared() const
	{
		return powf(w,2) + powf(x,2) + powf(y,2) + powf(z,2);
	}
	/** @return the magnitude of the vector before normalization */
	inline f32 normalize();
};
struct v2u32
{
	u32 x;
	u32 y;
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
internal v4f32 operator*(const m4x4f32& lhs, const v4f32& rhs);
namespace kmath
{
	using Quaternion = v4f32;
	global_variable const Quaternion IDENTITY_QUATERNION = {1,0,0,0};
	// Thanks, Bruce Dawson
	// Source: https://randomascii.wordpress.com/2012/02/25/comparing-floating-point-numbers-2012-edition/
	internal inline bool isNearlyEqual(f32 fA, f32 fB, f32 epsilon = 1e-5f)
	{
		const f32 diff = fabsf(fA - fB);
		fA = fabsf(fA);
		fB = fabsf(fB);
		const f32 largest = (fB > fA) ? fB : fA;
		return (diff <= largest * epsilon);
	}
	internal inline bool isNearlyZero(f32 f, f32 epsilon = 1e-5f)
	{
		return isNearlyEqual(f, 0.f, epsilon);
	}
	/** @return # of bytes which represent `k` kilobytes */
	internal inline u64 kilobytes(u64 k)
	{
		return k*1024;
	}
	/** @return # of bytes which represent `m` megabytes */
	internal inline u64 megabytes(u64 m)
	{
		return kilobytes(m)*1024;
	}
	/** @return # of bytes which represent `g` gigabytes */
	internal inline u64 gigabytes(u64 g)
	{
		return megabytes(g)*1024;
	}
	/** @return # of bytes which represent `t` terabytes */
	internal inline u64 terabytes(u64 t)
	{
		return gigabytes(t)*1024;
	}
	internal inline u32 safeTruncateU32(u64 value)
	{
		local_persist const u32 MAX_U32 = ~u32(0);
		kassert(value < MAX_U32);
		return static_cast<u32>(value);
	}
	internal inline u32 safeTruncateU32(i32 value)
	{
		local_persist const u32 MAX_U32 = ~u32(0);
		kassert(value >= 0 && value < MAX_U32);
		return static_cast<u32>(value);
	}
	internal inline u32 safeTruncateU32(i64 value)
	{
		local_persist const u32 MAX_U32 = ~u32(0);
		kassert(value >= 0 && value < MAX_U32);
		return static_cast<u32>(value);
	}
	internal inline i32 safeTruncateI32(u64 value)
	{
		local_persist const i32 MAX_I32 = ~i32(1<<31);
		kassert(value < MAX_I32);
		return static_cast<i32>(value);
	}
	internal inline u16 safeTruncateU16(i32 value)
	{
		local_persist const u16 MAX_U16 = static_cast<u16>(~u16(0));
		kassert(value >= 0 && value < MAX_U16);
		return static_cast<u16>(value);
	}
	internal inline u16 safeTruncateU16(u32 value)
	{
		local_persist const u16 MAX_U16 = static_cast<u16>(~u16(0));
		kassert(value < MAX_U16);
		return static_cast<u16>(value);
	}
	internal inline f32 v2Radians(const v2f32& v)
	{
		if(isNearlyZero(v.x) && isNearlyZero(v.y))
		{
			return 0;
		}
		return atan2f(v.y, v.x);
	}
	internal inline f32 radiansBetween(v2f32 v0, v2f32 v1, 
	                                   bool v0IsNormalized = false, 
	                                   bool v1IsNormalized = false)
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
	internal inline v3f32 cross(const v2f32& lhs, const v2f32& rhs)
	{
		return {0, 0, lhs.x*rhs.y - lhs.y*rhs.x};
	}
	internal inline v2f32 normal(v2f32 v)
	{
		v.normalize();
		return v;
	}
	internal inline f32 lerp(f32 min, f32 max, f32 ratio)
	{
		return min + ratio*(max - min);
	}
	internal inline v2f32 rotate(const v2f32& v, f32 radians)
	{
		v2f32 result = 
			{ .x = cosf(radians)*v.x - sinf(radians)*v.y
			, .y = sinf(radians)*v.x + cosf(radians)*v.y
		};
		return result;
	}
	internal inline Quaternion quat(v3f32 axis, f32 radians, 
	                                bool axisIsNormalized = false)
	{
		if(!axisIsNormalized)
		{
			axis.normalize();
		}
		Quaternion result;
		const f32 sine = sinf(radians/2);
		result.w = cosf(radians/2);
		result.x = sine * axis.x;
		result.y = sine * axis.y;
		result.z = sine * axis.z;
		return result;
	}
	// Source: https://en.wikipedia.org/wiki/Quaternion#Hamilton_product
	internal inline Quaternion quatHamilton(const Quaternion& q0, 
	                                        const Quaternion& q1)
	{
		return { q0.w*q1.w - q0.x*q1.x - q0.y*q1.y - q0.z*q1.z, 
		         q0.w*q1.x + q0.x*q1.w + q0.y*q1.z - q0.z*q1.y,
		         q0.w*q1.y - q0.x*q1.z + q0.y*q1.w + q0.z*q1.x,
		         q0.w*q1.z + q0.x*q1.y - q0.y*q1.x + q0.z*q1.w };
	}
	//Source: https://en.wikipedia.org/wiki/Quaternion#Conjugation,_the_norm,_and_reciprocal
	internal inline Quaternion quatConjugate(const Quaternion& q)
	{
		return {q.w, -q.x, -q.y, -q.z};
	}
	internal inline v2f32 quatTransform(Quaternion quat, 
	                                    const v2f32& v2d, 
	                                    bool quatIsNormalized = false)
	{
		if(!quatIsNormalized)
		{
			quat.normalize();
		}
		const Quaternion result = 
			quatHamilton(quatHamilton(quat, {0, v2d.x, v2d.y, 0}), 
			             quatConjugate(quat));
		return {result.x, result.y};
	}
	internal inline f32 quatNormalize(Quaternion* q)
	{
		return q->normalize();
	}
}
internal inline kmath::Quaternion operator*(const kmath::Quaternion& lhs, 
                                            const kmath::Quaternion& rhs)
{
	return kmath::quatHamilton(lhs, rhs);
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