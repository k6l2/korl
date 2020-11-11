#include "kgtRandom.h"
#include "kmath.h"
internal void
	kgtRandomSeed(KgtRandom* kgtRandom, u16 seed0, u16 seed1, u16 seed2)
{
	kgtRandom->seed[0] = seed0;
	kgtRandom->seed[1] = seed1;
	kgtRandom->seed[2] = seed2;
}
internal f32 
	kgtRandomF32_0_1(KgtRandom* kgtRandom)
{
	local_persist const u16 WICHMANN_HILL_CONSTS[] = { 30269, 30307, 30323 };
	kgtRandom->seed[0] = kmath::safeTruncateU16(
		(171 * kgtRandom->seed[0]) % WICHMANN_HILL_CONSTS[0]);
	kgtRandom->seed[1] = kmath::safeTruncateU16(
		(172 * kgtRandom->seed[1]) % WICHMANN_HILL_CONSTS[1]);
	kgtRandom->seed[2] = kmath::safeTruncateU16(
		(170 * kgtRandom->seed[2]) % WICHMANN_HILL_CONSTS[2]);
	return fmodf(kgtRandom->seed[0]/static_cast<f32>(WICHMANN_HILL_CONSTS[0]) + 
	             kgtRandom->seed[1]/static_cast<f32>(WICHMANN_HILL_CONSTS[1]) + 
	             kgtRandom->seed[2]/static_cast<f32>(WICHMANN_HILL_CONSTS[2]), 
	             1);
}