#pragma once
#include "kutil.h"
#include "platform-game-interfaces.h"
#include "krb-interface.h"
#include "kgtAllocator.h"
#include "kgtAssetManager.h"
#include "kgtAudioMixer.h"
#include "kgtVertex.h"
#include "kgtDraw.h"
#pragma warning( push )
	// warning C4820: bytes padding added after data member
	#pragma warning( disable : 4820 )
	#include "imgui/imgui.h"
#pragma warning( pop )
internal void* kStbDsRealloc(void* allocatedAddress, size_t newAllocationSize, 
                             void* context);
internal void kStbDsFree(void* allocatedAddress, void* context);
struct KgtGameState
{
	/* memory allocators */
	KgtAllocatorHandle hKalPermanent;
	KgtAllocatorHandle hKalTransient;
	KgtAllocatorHandle hKalFrame;
	/* required state for KRB */
	krb::Context krbContext;
	/* useful utilities which almost every game will use */
	KgtAssetManager* assetManager;
	KgtAudioMixer* audioMixer;
};
/* useful global variables */
global_variable KmlPlatformApi* g_kpl;
global_variable KrbApi* g_krb;
global_variable KgtAssetManager* g_kam;
global_variable KgtGameState* g_kgs;
/* KGT game state API */
internal void 
	kgtGameStateOnReloadCode(GameMemory& memory);
/** 
 * Make sure to clear (zero-out) ALL game state memory, including the memory 
 * occupied by `tgs` BEFORE calling this function!!!
 */
internal void 
	kgtGameStateInitialize(
		KgtGameState* kgs, GameMemory& memory, size_t totalGameStateSize);
internal void 
	kgtGameStateRenderAudio(
		KgtGameState* kgs, GameAudioBuffer& audioBuffer, 
		u32 sampleBlocksConsumed);
/** @return true to request the platform continues to run, false to request the 
 *          platform closes the application */
internal bool 
	kgtGameStateUpdateAndDraw(
		KgtGameState* kgs, const GameKeyboard& gameKeyboard, 
		bool windowIsFocused);
/* convenience macros for our application */
#define ALLOC_FRAME_ARRAY(type,elements) \
	reinterpret_cast<type*>(\
		kgtAllocAlloc(g_kgs->hKalFrame,sizeof(type)*(elements)))
