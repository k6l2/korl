#pragma once
#include "kutil.h"
#include "kgtAllocator.h"
#include "kgtActor.h"
using KgtDirectorActorId = u16;
struct KgtDirector
{
	KgtAllocatorHandle hKal;// needed for destruction memory cleanup
	KgtDirectorActorId maxActors;
	struct ActorRole
	{
		u16 salt;
		enum class State : u8
			{ VACANT
			, HIRE_QUEUED
			, HIRED
			, FIRED } state;
	} *roles;
	KgtActor* actors;
	f32 frameSeconds;// updated during director step
};
internal void 
	kgtDirectorConstruct(
		KgtDirector* d, KgtAllocatorHandle hKal, KgtDirectorActorId maxActors, 
		KgtDirectorActorId maxEmployQueueSize);
/**
 * 
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
	kgtDirectorStep(KgtDirector* d, f32 frameSeconds);