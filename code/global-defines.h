#pragma once
#define internal        static
#define local_persist   static
#define global_variable static
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
};
namespace kmath
{
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
}