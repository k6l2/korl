#pragma once
#include "kutil.h"
#include "kgtBodyCollider.h"
using KgtActorType   = u16;
using KgtActorHandle = u64;
enum class KgtActorRootType : u8
	/* a root type of BODY means the actor's world position is determined by a 
		handle to a KgtBodyColliderBody */
	{ BODY
	/* a root type of RAW means the actor's world position is stored locally in 
		position/orientation vectors.  In other words, the actor has no geometry 
		associated with it */
	, RAW };
#define KGT_POLYMORPHIC_TAGGED_UNION()
#define KGT_POLYMORPHIC_TAGGED_UNION_EXTENDS(superStruct)
/* we need to include the headers of the datatypes which belong to the 
	polymorphic tagged union defined below */
#include "gen_ptu_KgtActor_includes.h"
KGT_POLYMORPHIC_TAGGED_UNION()
struct KgtActor
{
	/** KgtActorType:u16 << 48 | pool_index:u32 << 16 | salt:u16 */
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