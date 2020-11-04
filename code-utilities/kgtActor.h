#include "kutil.h"
#include "kgtBodyCollider.h"
using KgtActorType   = u16;
using KgtActorHandle = u64;
enum class KgtActorRootType : u8
	{ BODY, RAW };
#define KGT_POLYMORPHIC_TAGGED_UNION()
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
#if 0
	#include "gen_kgtActor.h"
#else
	union 
	{
		void* no_derived_structs;
	};
#endif
};