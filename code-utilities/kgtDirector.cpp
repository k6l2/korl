#include "kgtDirector.h"
internal KgtActorHandle 
	kgtDirectorActorHandleMake(KgtDirector* d, KgtDirectorActorId roleId)
{
	return (KgtActorHandle(roleId + 1) << 16) 
	      | KgtActorHandle(d->roles[roleId].salt);
}
/** @return false if the role index of `hActor` is invalid */
internal bool 
	kgtDirectorActorHandleParse(
		KgtActorHandle* hActor, KgtDirectorActorId* o_roleId, u16* o_salt)
{
	KgtDirectorActorId rawRoleId = *hActor >> 16;
	if(rawRoleId == 0)
	{
		*hActor = 0;
		return false;
	}
	*o_roleId = kmath::safeTruncateU16(rawRoleId - 1);
	*o_salt   = *hActor & 0xFFFF;
	return true;
}
internal void 
	kgtDirectorConstruct(
		KgtDirector* d, KgtAllocatorHandle hKal, KgtDirectorActorId maxActors)
{
	/* Ensure that the pool index of the actor handle will always fit in a 
		16-bit number if incrememnted by 1.  We do this so that a cleared actor 
		handle is always invalid. */
	korlAssert(maxActors < 0xFFFF);
	*d = {};
	d->hKal      = hKal;
	d->maxActors = maxActors;
	d->roles = reinterpret_cast<KgtDirector::Role*>(
		kgtAllocAlloc(hKal, maxActors*sizeof(*d->roles)));
	d->actors = reinterpret_cast<KgtActor*>(
		kgtAllocAlloc(hKal, maxActors*sizeof(*d->actors)));
	/* construct the data structure used to calculate geometry collision */
	{
		const KgtBodyColliderMemoryRequirements bcMemReqs = 
			{ .maxShapes             = maxActors
			, .maxBodies             = 
				kmath::safeTruncateU32(2*maxActors)
			, .maxCollisionManifolds = 
				kmath::safeTruncateU32(maxActors*maxActors) };
		const size_t bcBytes = kgtBodyColliderRequiredBytes(bcMemReqs);
		void*const bcMemoryStart = kgtAllocAlloc(hKal, bcBytes);
		d->bodyCollider = kgtBodyColliderConstruct(bcMemoryStart, bcMemReqs);
	}
	for(size_t a = 0; a < maxActors; a++)
	{
		d->actors[a].type = KgtActor::Type::ENUM_COUNT;
		d->roles[a] = {};
	}
}
internal KgtActor* 
	kgtDirectorHire(KgtDirector* d, KgtActor::Type actorType)
{
#if 0
	/* instead of immediately creating a new actor in the role slot, we add the 
		actor to an employment queue, and at the end of the next call to step we 
		actually add the actor to the roll????
		Or actually, maybe we DO in fact add the actor, but it is simply marked 
		as QUEUED until the end of the next step function */
	korlAssert(!"TODO"); return nullptr;
#else
	KgtDirectorActorId freeRoleId = 0;
	for(; freeRoleId < d->maxActors; freeRoleId++)
	{
		if(d->roles[freeRoleId].state != KgtDirector::Role::State::VACANT)
			continue;
		break;
	}
	if(freeRoleId >= d->maxActors)
	{
		KLOG(WARNING, "Director is full!");
		return nullptr;
	}
	korlAssert(d->roles[freeRoleId].state == KgtDirector::Role::State::VACANT);
	d->roles[freeRoleId].state = KgtDirector::Role::State::HIRE_QUEUED;
	d->roles[freeRoleId].salt++;
	KgtActor*const result = &d->actors[freeRoleId];
	korlAssert(result->type == KgtActor::Type::ENUM_COUNT);
	korlAssert(actorType     < KgtActor::Type::ENUM_COUNT);
	result->type   = actorType;
	result->handle = kgtDirectorActorHandleMake(d, freeRoleId);
	kgtActorInitialize(result, d);
	return result;
#endif
}
internal void 
	kgtDirectorFire(KgtDirector* d, KgtActorHandle* hActor)
{
	defer(*hActor = 0;);
	KgtDirectorActorId roleId;
	u16 actorSalt;
	if(!kgtDirectorActorHandleParse(hActor, &roleId, &actorSalt))
	{
		KLOG(WARNING, "Attempted to fire actor using invalid handle.");
		return;
	}
	korlAssert(roleId < d->maxActors);
	if(d->roles[roleId].salt != actorSalt)
	{
		KLOG(WARNING, "Attempted to fire stale actor (salt=%i, handleSalt=%i).", 
		     d->roles[roleId].salt, actorSalt);
		return;
	}
	/* we can only fire HIRED or HIRED_QUEUED actors */
	if(   d->roles[roleId].state == KgtDirector::Role::State::FIRED 
	   || d->roles[roleId].state == KgtDirector::Role::State::VACANT)
	{
		KLOG(WARNING, "Attempted to fire vacant actor (roleId=%i).", roleId);
		return;
	}
	d->roles[roleId].state = KgtDirector::Role::State::FIRED;
}
internal KgtActor* 
	kgtDirectorGetActor(KgtDirector* d, KgtActorHandle* hActor)
{
	KgtDirectorActorId roleId;
	u16 actorSalt;
	if(!kgtDirectorActorHandleParse(hActor, &roleId, &actorSalt))
	{
		KLOG(WARNING, "Attempted to get actor using invalid handle.");
		return nullptr;
	}
	korlAssert(roleId < d->maxActors);
	if(d->roles[roleId].salt != actorSalt)
	{
		KLOG(WARNING, "Attempted to get stale actor (salt=%i, handleSalt=%i).", 
		     d->roles[roleId].salt, actorSalt);
		return nullptr;
	}
	KgtActor*const result = &d->actors[roleId];
	return result;
}
internal void 
	kgtDirectorStep(
		KgtDirector* d, f32 frameSeconds, KgtAllocatorHandle hKalTemp)
{
	/* update collision detection system */
	kgtBodyColliderUpdateManifolds(d->bodyCollider, hKalTemp);
	/* step HIRED actors */
	/* @speed: use a temporary allocator to cache a list of pointers to hired 
		actors */
	/* @speed: maintain a hired actor count & stop iterating once we reached 
		that # */
	for(KgtDirectorActorId a = 0; a < d->maxActors; a++)
	{
		if(d->roles[a].state != KgtDirector::Role::State::HIRED)
			continue;
		kgtActorStep(&d->actors[a], d);
	}
	/* draw HIRED actors */
	/* @speed: use the cached list of hired actors retrieved from the previous 
		loop to waste less iteration here */
	/* @speed: use a temporary allocator to cache a list of pointers to fired & 
		hire_queued actors */
	for(KgtDirectorActorId a = 0; a < d->maxActors; a++)
	{
		if(d->roles[a].state != KgtDirector::Role::State::HIRED)
			continue;
		kgtActorDraw(&d->actors[a], d);
	}
	/* nullify fired actors */
	/* @speed: use the cached list of fired actors retrieved from the draw loop 
		to waste less iteration here */
	for(KgtDirectorActorId a = 0; a < d->maxActors; a++)
	{
		if(d->roles[a].state != KgtDirector::Role::State::FIRED)
			continue;
		kgtActorOnDestroy(&d->actors[a], d);
		d->roles[a].state = KgtDirector::Role::State::VACANT;
		d->actors[a].type = KgtActor::Type::ENUM_COUNT;
	}
	/* confirm hire queued actors */
	/* @speed: use the cached list of hire_queued actors retrieved from the draw 
		loop to waste less iteration here */
	for(KgtDirectorActorId a = 0; a < d->maxActors; a++)
	{
		if(d->roles[a].state != KgtDirector::Role::State::HIRE_QUEUED)
			continue;
		d->roles[a].state = KgtDirector::Role::State::HIRED;
	}
}