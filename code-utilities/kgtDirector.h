#pragma once
#include "kutil.h"
#include "kgtAllocator.h"
#include "kgtActor.h"
#include "kgtBodyCollider.h"
using KgtDirectorActorId = u16;
struct KgtDirector
{
	KgtAllocatorHandle hKal;// needed for destruction memory cleanup
	KgtDirectorActorId maxActors;
	struct Role
	{
		u16 salt;
		/* only [HIRED | FIRED] actors will perform work during step */
		enum class State : u8
			{ VACANT
			/* queued actors will NOT perform work, and become hired at the end 
				of the current/next call to step */
			, HIRE_QUEUED
			/* hired actors will always perform work during step */
			, HIRED
			/* fired actors will STILL perform work for the duration of the next 
				call to step, and will subsequently be nullified */
			, FIRED } state;
	} *roles;
	KgtActor* actors;
	KgtBodyCollider* bodyCollider;
	f32 frameSeconds;// updated during director step
};
internal void 
	kgtDirectorConstruct(
		KgtDirector* d, KgtAllocatorHandle hKal, KgtDirectorActorId maxActors);
/**
 * @return A pointer to the actor data.
 */
internal KgtActor* 
	kgtDirectorHire(KgtDirector* d, KgtActor::Type actorType);
/**
 * actors flagged to be fired do not cease to exist until the end of the next 
 * call to step.
 */
internal void 
	kgtDirectorFire(KgtDirector* d, KgtActorHandle* hActor);
internal KgtActor* 
	kgtDirectorGetActor(KgtDirector* d, KgtActorHandle* hActor);
internal void 
	kgtDirectorStep(
		KgtDirector* d, f32 frameSeconds, KgtAllocatorHandle hKalTemp);