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
		KgtDirector* d, KgtAllocatorHandle hKal, KgtDirectorActorId maxActors, 
		KgtDirectorActorId maxEmployQueueSize)
{
	/* ensure that the pool index of the actor handle will always fit in a 
		16-bit number if incrememnted by 1 */
	korlAssert(maxActors < 0xFFFF);
	*d = {};
	d->hKal            = hKal;
	d->maxActors       = maxActors;
	d->roles = reinterpret_cast<KgtDirector::ActorRole*>(
		kgtAllocAlloc(hKal, maxActors*sizeof(*d->roles)));
	d->actors = reinterpret_cast<KgtActor*>(
		kgtAllocAlloc(hKal, maxActors*sizeof(*d->actors)));
	for(size_t a = 0; a < maxActors; a++)
	{
		d->actors[a].type = KgtActor::Type::ENUM_COUNT;
		d->roles[a] = {};
	}
}
internal KgtActor* 
	kgtDirectorHire(KgtDirector* d, KgtActor::Type actorType)
{
#if 1
	/* instead of immediately creating a new actor in the role slot, we add the 
		actor to an employment queue, and at the end of the next call to step we 
		actually add the actor to the roll */
	korlAssert(!"TODO"); return nullptr;
#else
	KgtDirectorActorId freeRoleId = 0;
	for(; freeRoleId < d->maxActors; freeRoleId++)
	{
		if(d->roles[freeRoleId].occupied)
			continue;
		break;
	}
	if(freeRoleId >= d->maxActors)
	{
		KLOG(WARNING, "Director is full!");
		return nullptr;
	}
	korlAssert(!d->roles[freeRoleId].occupied);
	d->roles[freeRoleId].occupied = true;
	d->roles[freeRoleId].fired    = false;
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
		KLOG(WARNING, "Attempted to fire actor using invalid handle!");
		return;
	}
	korlAssert(roleId < d->maxActors);
	if(   d->roles[roleId].salt != actorSalt 
	   || d->roles[roleId].state == KgtDirector::ActorRole::State::VACANT)
	{
		return;
	}
	d->roles[roleId].state = KgtDirector::ActorRole::State::FIRED;
}
internal KgtActor* 
	kgtDirectorGetActor(KgtDirector* d, KgtActorHandle* hActor)
{
	korlAssert(!"TODO");return nullptr;
}
internal void 
	kgtDirectorStep(KgtDirector* d, f32 frameSeconds)
{
	korlAssert(!"TODO");
}