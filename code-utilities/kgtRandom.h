/* simple implementation of Wichmann-Hill algorithm for RNG - sufficient enough 
	for video games */
#pragma once
#include "kutil.h"
struct KgtRandom
{
	u16 seed[3];
};
/** @param seedX these parameters should fall in the random range [1,30'000] */
internal void
	kgtRandomSeed(KgtRandom* kgtRandom, u16 seed0, u16 seed1, u16 seed2);
internal f32 
	kgtRandomF32_0_1(KgtRandom* kgtRandom);