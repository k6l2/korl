#pragma once
#define internal        static
#define local_persist   static
#define global_variable static
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
	#define kassert(expression) {}
#endif
// These must be re-defined in the appropriate compilation units //
#define KLOG(platformLogCategory, formattedString, ...)
// KC++ support //
//	IMPORTANT!!!  Do not use the values defined here for actual logic.  These 
//	macros are pre-defined with specific values only to shut up the compiler/
//	intellisense.  When kc++ runs over the source code containing these macros,
//	they are replaced with actual useful values.
using KAssetCStr = const char*const*;
#define KASSET(cstrFileName) nullptr
#define KASSET_INDEX(KAssetCStr) 0
#define KASSET_COUNT 0
#define KASSET_TYPE(KAssetCStr) 0
#define KASSET_TYPE_PNG 0
#define KASSET_TYPE_WAV 1
#define KASSET_TYPE_OGG 2
#define KASSET_TYPE_UNKNOWN 3
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
	inline v2f32 operator-()
	{
		return v2f32{-x,-y};
	}
};
struct v3f32
{
	f32 x, y, z;
};
struct v4f32
{
	f32 x, y, z, w;
};
struct v2u32
{
	u32 x;
	u32 y;
};
namespace kmath
{
	using Quaternion = v4f32;
	global_variable const Quaternion IDENTITY_QUATERNION = {0,0,0,1};
	// Thanks, Micha Wiedenmann
	// Derived from: https://stackoverflow.com/q/19837576
	internal inline bool isNearlyEqual(f32 fA, f32 fB)
	{
		local_persist const f32 EPSILON = 1e-5f;
		return fabsf(fA - fB) <= EPSILON * fabsf(fA);
	}
	internal inline bool isNearlyZero(f32 f)
	{
		local_persist const f32 EPSILON = 1e-5f;
		return isNearlyEqual(f, 0.f);
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
	internal inline f32 v2Radians(const v2f32& v)
	{
		if(isNearlyZero(v.x) && isNearlyZero(v.y))
		{
			return 0;
		}
		return atan2f(v.y, v.x);
	}
	internal inline Quaternion quat(const v3f32& axis, f32 radians)
	{
		Quaternion result;
		const f32 sine = sinf(radians/2);
		result.w = cosf(radians/2);
		result.x = sine * axis.x;
		result.y = sine * axis.y;
		result.z = sine * axis.z;
		return result;
	}
}