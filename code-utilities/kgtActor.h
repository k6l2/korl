#pragma once
#include "kutil.h"
#include "kgtBodyCollider.h"
/** pool_index:u16 << 16 | salt:u16 */
using KgtActorHandle = u32;
enum class KgtActorRootType : u8
	/* a root type of BODY means the actor's world position is determined by a 
		handle to a KgtBodyColliderBody */
	{ BODY
	/* a root type of RAW means the actor's world position is stored locally in 
		position/orientation vectors.  In other words, the actor has no geometry 
		associated with it */
	, RAW };
#include "kcppPolymorphicTaggedUnion.h"
/* we need to include the headers of the datatypes which belong to the 
	polymorphic tagged union defined below */
#include "gen_ptu_KgtActor_includes.h"
KCPP_POLYMORPHIC_TAGGED_UNION struct KgtActor
{
	KgtActorHandle handle;
#if 0
	/* This should probably just be handled & queried separately from the 
		Director, or whatever manages Actors */
	bool fired;
#endif// 0
	union
	{
		KgtActorRootType type;
		struct 
		{
			KgtActorRootType type;
			KgtBodyColliderBodyHandle handle;
		}body;
		struct 
		{
			KgtActorRootType type;
			v3f32 position;
			q32 orient;
		}raw;
	}root;
	/** Some kind of meta-programming pass should generate a tagged union for 
	 * "derived" structures here.  For example, if we have two derived structs 
	 * "Ball" & "Loop", it would look like:
	 * union
	 * {
	 * 	Ball ball;
	 * 	Loop loop;
	 * }; */
	#include "gen_ptu_KgtActor.h"
};
/* @TODO: declare & define virtual callbacks that use the above polymorphic 
	tagged union declared above, as well as "derived" structs.  Perhaps such a 
	system should only support one level of inheritance for the sake of 
	simplicity? */
KCPP_POLYMORPHIC_TAGGED_UNION_PURE_VIRTUAL internal void 
	kgtActorInitialize(KgtActor* a, void* directorData);
KCPP_POLYMORPHIC_TAGGED_UNION_PURE_VIRTUAL internal void 
	kgtActorOnDestroy(KgtActor* a, void* directorData);
KCPP_POLYMORPHIC_TAGGED_UNION_PURE_VIRTUAL internal void 
	kgtActorStep(KgtActor* a, void* directorData);
KCPP_POLYMORPHIC_TAGGED_UNION_PURE_VIRTUAL internal void 
	kgtActorDraw(KgtActor* a, void* directorData);